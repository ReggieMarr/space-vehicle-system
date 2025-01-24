#!/bin/bash
export SCRIPT_DIR="$(cd "$(dirname "$0")"; pwd -P)"
cd "$SCRIPT_DIR"
source .env
set -e
set -o pipefail

check_port() {
    local port=$1
    if netstat -ano | grep ":$port "; then
        echo "Port $port is in use. Checking its state..."
        if netstat -ano | grep ":$port.*TIME_WAIT"; then
            echo "Port $port is in TIME_WAIT state. Wait $(cat /proc/sys/net/ipv4/tcp_fin_timeout) seconds, or use a different port."
        else
            echo "Port $port is actively in use by another process. Use lsof -i :$port to find the process."
        fi
        return 1
    fi
    return 0
}

show_help() {
  cat << EOF
Usage: $(basename "$0") [OPTIONS] COMMAND

Options:
  --daemon             Run as daemon
  --debug              Enable debug mode
  --as-host            Run as host
  --clean              Clean build
  --host-thread-ctl    Set thread control for running without sudo (this itself requires sudo)
  --help               Show this help message
Commands:
  docker-build         Build the Docker image
  build                Build the project
  format               Leverage fprime-util's clang-format to format project
  inspect [container]  Inspect a container
  exec gds             Run the GDS
  exec FlightComputer  Run the Flight Software
  exec test            Run integration tests against the Flight Software
  update               Pull latest Docker images
  teardown             Tear down the environment
  topology             Generate topology visualization
EOF
}

CLEAN=0
AS_HOST=0
DAEMON=0
DEBUG=0
SET_THREAD_CTRL=0
FORCE=0
STANDALONE=0

# Process flags
for arg in "$@"; do
  case $arg in
    --daemon) DAEMON=1 ;;
    --force) FORCE=1 ;;
    --debug) DEBUG=1 ;;
    --as-host) AS_HOST=1 ;;
    --clean) CLEAN=1 ;;
    --host-thread-ctrl) SET_THREAD_CTRL=1 ;;
    --host-thread-ctrl) STANDALONE=1 ;;
    --help) show_help; exit 0 ;;
  esac
done

export FSW_IMG_BASE=${FSW_IMG_BASE:-"ghcr.io/reggiemarr/fprime-flight-stack"}
export FSW_IMG_TAG=${FSW_IMG_TAG:-fsw_$(git rev-parse --abbrev-ref HEAD | sed 's/\//_/g')}
export FSW_IMG="$FSW_IMG_BASE:$FSW_IMG_TAG"

exec_cmd() {
    local cmd="$1"
    echo "$cmd"
    eval "$cmd"
    exit_code=$?
    if [ $exit_code -eq 1 ] || [ $exit_code -eq 2 ]; then
        echo "Failed cmd with error $exit_code"
        exit $exit_code;
    fi
}

run_docker_compose() {
    local cmd="$1"
    # Always kill the container after executing the command
    # by default run the command with an interactive tty
    local flags="--rm --user $(id -u):$(id -g) --remove-orphans "

    if [ "${DAEMON}" -eq "1" ]; then
      flags+="-id "
    else
      flags+="-it "
    fi

    echo $flags $2
    # If flags were passed then add them
    flags+=${2:-}

    [ "$STANDALONE" -eq 1 ] && flags="--no-deps"

    exec_cmd "docker compose run $flags $cmd"
}

stop_container() {
    local container_name="$1"
    local timeout=${2:-10}  # default 10 seconds timeout

    if docker container inspect "$container_name" >/dev/null 2>&1; then
        echo "Stopping container $container_name (timeout: ${timeout}s)..."
        if ! docker compose down -t "$timeout" "$container_name"; then
            echo "Failed to stop container $container_name gracefully"
            return 1
        fi
        echo "Container $container_name stopped successfully"
    else
        echo "Container $container_name is not running"
    fi
}

exec_fsw() {
    local target="$1"
    local bin="${FSW_WDIR}/${target}/build-artifacts/Linux/${target}/bin/${target}"
    local cmd="$bin -a ${GDS_IP} -u ${UPLINK_TARGET_PORT} -d ${DOWNLINK_TARGET_PORT}"
    [ "$DEBUG" -eq 1 ] && cmd="gdbserver :${GDB_PORT} ${cmd}"

    # Handle clean restart of GDS if requested
    if [ "$CLEAN" -eq 1 ] && [ "$STANDALONE" -eq 0 ]; then
        # Stop GDS container with a 3-second timeout (matching your compose file's grace period)
        # stop_container "ground-control" 3

        if ! docker container stop -t 3 "ground-control"; then
            echo "Failed to stop container gds gracefully"
            return 1
        fi
    fi

    if [ "${AS_HOST}" -eq "1" ]; then
        exec_cmd "$cmd"
    else
        run_docker_compose "fsw bash -c \"${cmd}\""
    fi
}

case $1 in
  "sync")
    exec_cmd "docker push $FSW_IMG"
    ;;

  "update")
    PULL_CMD="git pull"
    [ "$FORCE" -eq 1 ] && PULL_CMD="git fetch -a && git reset --hard origin/$(git rev-parse --abbrev-ref HEAD)"
    PULL_CMD+=" && git submodule sync && git submodule update --init --recursive"

    exec_cmd "$PULL_CMD"
    ;;

  "docker-build")
    if ! git diff-index --quiet HEAD --; then
        read -p "You have unstaged changes. Continue? (y/n) " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || { echo "Build cancelled."; exit 1; }
    fi

    # Fetch from remote to ensure we have latest refs
    git fetch -q origin

    # Get current commit hash
    CURRENT_COMMIT=$(git rev-parse HEAD)

    # Check if current commit exists in any remote branch
    if ! git branch -r --contains "$CURRENT_COMMIT" | grep -q "origin/"; then
        read -p "Current commit not found in remote repository. Continue? (y/n) " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || { echo "Build cancelled."; exit 1; }
    fi

    CMD="docker compose --progress=plain --env-file=${SCRIPT_DIR}/.env build fsw"
    [ "$CLEAN" -eq 1 ] && CMD+=" --no-cache"
    CMD+=" --build-arg GIT_COMMIT=$(git rev-parse HEAD) --build-arg GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)"
    exec_cmd "$CMD"
    ;;

  "format")
    if [[ "$2" == *.fpp ]]; then
      container_file="${2/$SCRIPT_DIR/$FSW_WDIR}"
      echo "Formatting FPP file: $container_file"
      cmd="fpp-format $container_file"
      # Create a temporary marker that's unlikely to appear in normal code
      marker="@ COMMENT_PRESERVE@"
      # Chain the commands:
      # 1. Transform comments to temporary annotations
      # 2. Run fpp-format
      # 3. Transform back to comments
      # 4. Write back to the original file
    tmp_file="${container_file/.fpp/_tmp.fpp}"

    # Create a multi-line command with error checking
    read -r -d '' cmd <<EOF
    set -e  # Exit on any error

    # Create backup
    cp "$container_file" "${container_file}.bak"

    # Attempt formatting pipeline
    if sed 's/^\\([ ]*\\)#/\\1${marker}#/' "$container_file" \
       | fpp-format \
       | sed 's/^\\([ ]*\\)${marker}#/\\1#/' > "$tmp_file"; then

        # If successful, verify tmp file exists and has content
        if [ -s "$tmp_file" ]; then
            mv "$tmp_file" "$container_file"
            rm "${container_file}.bak"
            echo "Format successful"
        else
            echo "Error: Formatted file is empty"
            mv "${container_file}.bak" "$container_file"
            exit 1
        fi
    else
        echo "Error during formatting"
        mv "${container_file}.bak" "$container_file"
        [ -f "$tmp_file" ] && rm "$tmp_file"
        exit 1
    fi
EOF
      run_docker_compose "fsw bash -c \"$cmd\"" " -w $FSW_WDIR"
    else
      fprime_root="${2:-$SCRIPT_DIR/fprime}"  # Get the path provided or use current directory
      fprime_root="${fprime_root/$SCRIPT_DIR/$FSW_WDIR}"
      echo "Formatting from $fprime_root"
      cmd="git diff --name-only --relative | fprime-util format --no-backup --stdin"
      run_docker_compose "fsw bash -c \"$cmd\"" "-w $fprime_root"
    fi
    ;;

  "build")
    BUILD_CMD="fprime-util build -j10 --all"

    [ "$CLEAN" -eq 1 ] && BUILD_CMD="fprime-util purge --force && fprime-util generate && $BUILD_CMD"
    [ "$AS_HOST" -eq 1 ] && eval "$BUILD_CMD" || run_docker_compose "fsw bash -c \"$BUILD_CMD\""

    MOD_DICT_CMD="sed -i \"s|${FSW_WDIR}|${SCRIPT_DIR}|g\" \"${SCRIPT_DIR}/FlightComputer/build-fprime-automatic-native/compile_commands.json\""

    exec_cmd "$MOD_DICT_CMD"

    if [ "${SET_THREAD_CTRL}" -eq "1" ]; then
        echo 'Setting thread control for non-sudo host execution'
        THREAD_CMD="sudo setcap \"cap_sys_nice+ep\" ${SCRIPT_DIR}/FlightComputer/build-artifacts/Linux/FlightComputer/bin/FlightComputer"
        exec_cmd "$THREAD_CMD"
    fi
    ;;

  "inspect")
    SERVICE_NAME=${2:-}
    [ -z "$SERVICE_NAME" ] && { echo "Error: must specify container to inspect"; exit 1; }
    SERVICE_NAME+="${DEVICE_PORT:+-with-device}"
    run_docker_compose "$SERVICE_NAME bash"
    ;;

  "exec")
    EXEC_TARGET=${2:-}
    [ -z "$EXEC_TARGET" ] && { echo "Error: must specify target to exec"; exit 1; }

    case $EXEC_TARGET in
      "FlightComputer")
        exec_fsw "$EXEC_TARGET"
      ;;
      "gds")
        dict_path="${DICT_DIR}FlightComputerTopologyDictionary.json"
        docker_flags="--name ground-control"
        gds_flags=" --dictionary ${dict_path}"
        gds_flags+=" --no-app"
        gds_flags+=" --ip-address 127.0.0.1 --ip-port=${UPLINK_TARGET_PORT} --tts-port=${DOWNLINK_TARGET_PORT}"

        cmd="fprime-gds ${gds_flags}"
        run_docker_compose "gds bash -c \"$cmd\"" "${docker_flags}"
      ;;
      "test")
        run_docker_compose "gds pytest -s -v"
      ;;
      *)
      echo "Invalid operation."
      exit 1
      ;;
    esac
    ;;

  "topology")
    # NOTE set working dir when we link this to a CI/CD
    CMD="fprime-util visualize -p ${DEPLOYMENT_ROOT}/Top -r ${DEPLOYMENT_ROOT}"
    run_docker_compose "fsw bash -c \"$CMD\""
    ;;

  "teardown")
    echo "Tearing down FSW..."
    docker compose down
    ;;

  *)
    echo "Invalid operation."
    exit 1
    ;;
esac
