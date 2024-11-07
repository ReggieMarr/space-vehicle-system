#!/bin/bash
source .env
export SCRIPT_DIR="$(cd "$(dirname "$0")"; pwd -P)"
cd "$SCRIPT_DIR"
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
  --help               Show this help message

Commands:
  docker-build         Build the Docker image
  build                Build the project
  inspect [container]  Inspect a container
  gds                  Run the GDS
  update               Pull latest Docker images
  teardown             Tear down the environment
EOF
}

CLEAN=0
AS_HOST=0
DAEMON=0
DEBUG=0

# Process flags
for arg in "$@"; do
  case $arg in
    --daemon) DAEMON=1 ;;
    --debug) DEBUG=1 ;;
    --as-host) AS_HOST=1 ;;
    --clean) CLEAN=1 ;;
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
    local flags="-it --rm"
    flags+=" --user $(id -u):$(id -g)"

    [ "$DAEMON" -eq 1 ] && flags="-i -d"

    exec_cmd "docker compose run $flags $cmd"
}

exec_fsw() {
    local target="$1"
    local bin="${FSW_WDIR}/${target}/build-artifacts/Linux/${target}/bin/${target}"
    local cmd="$bin -a ${GDS_IP} -u ${UPLINK_TARGET_PORT} -d ${DOWNLINK_TARGET_PORT}"

    [ "$DEBUG" -eq 1 ] && cmd="gdbserver :${GDB_PORT} ${cmd}"

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
    exec_cmd "git submodule sync && git submodule update --init --recursive && docker pull $FSW_IMG"
    ;;

  "docker-build")
    if ! git diff-index --quiet HEAD --; then
        read -p "You have unstaged changes. Continue? (y/n) " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || { echo "Build cancelled."; exit 1; }
    fi

    if [ "$(git rev-parse HEAD)" != "$(git ls-remote $(git rev-parse --abbrev-ref @{u} | sed 's/\// /g') | cut -f1)" ]; then
        read -p "Current commit not pushed. Continue? (y/n) " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || { echo "Build cancelled."; exit 1; }
    fi

    CMD="docker compose --progress=plain --env-file=${SCRIPT_DIR}/.env build fsw"
    [ "$CLEAN" -eq 1 ] && CMD+=" --no-cache"
    CMD+=" --build-arg GIT_COMMIT=$(git rev-parse HEAD) --build-arg GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)"
    exec_cmd "$CMD"
    ;;

  "build")
    BUILD_CMD="fprime-util build -j10 --all"

    [ "$CLEAN" -eq 1 ] && BUILD_CMD="fprime-util purge --force && fprime-util generate && $BUILD_CMD"
    [ "$AS_HOST" -eq 1 ] && eval "$BUILD_CMD" || run_docker_compose "fsw bash -c \"$BUILD_CMD\""

    MOD_DICT_CMD="sed -i \"s|/fsw|${SCRIPT_DIR}/FlightComputer|g\" \"${SCRIPT_DIR}/FlightComputer/build-fprime-automatic-native/compile_commands.json\""

    exec_cmd "$MOD_DICT_CMD"
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
        # check_port ${DOWNLINK_TARGET_PORT}
        # check_port ${UPLINK_TARGET_PORT}

        DICT_PATH="${DICT_DIR}FlightComputerTopologyDictionary.json"
        FLAGS+=" --dictionary ${DICT_PATH}"
        FLAGS+=" --no-app"
        FLAGS+=" --ip-address 127.0.0.1 --ip-port=${UPLINK_TARGET_PORT} --tts-port=${DOWNLINK_TARGET_PORT}"

        GDS_CMD="fprime-gds ${FLAGS}"
        run_docker_compose "fsw bash -c \"$GDS_CMD\""
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

  "teardown")
    echo "Tearing down FSW..."
    docker compose down
    ;;

  *)
    echo "Invalid operation."
    exit 1
    ;;
esac
