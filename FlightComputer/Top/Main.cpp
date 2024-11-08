#include "Fw/Logger/Logger.hpp"
#include <getopt.h>
#include <cstdlib>
#include <ctype.h>

#include <Os/Console.hpp>

#include <Fw/Time/Time.hpp>
#include <FlightComputer/Top/FlightComputerTopologyAc.hpp>
#include <FlightComputer/Top/FlightComputerTopology.hpp>

#include <signal.h>
#include <cstdio>
#include <cstring>


void print_usage(const char* app) {
    (void) printf("Usage: ./%s [options]\n"
                  "-p, --persist\t\tstay up regardless of component failure\n"
                  "-d, --downlink PORT\tset downlink port\n"
                  "-u, --uplink PORT\tset uplink port\n"
                  "-a, --address HOST\tset hostname/IP address\n"
                  "-h, --help\t\tshow this help message\n", app);
}

volatile sig_atomic_t terminate = 0;
bool persist = false;

enum {
    EXIT_CODE_OK = 0,
    EXIT_CODE_STARTUP_FAILURE,
    EXIT_CODE_INIT_FAILURE,
    EXIT_CODE_RUNTIME_FAILURE,
};

static volatile int EXIT_RET = EXIT_CODE_OK;

static void initFailureSigHandler(int signum) {
    if (persist) {
        printf("Component failure detected in persist mode so no shutdown\n");
        return;
    }

    printf("Component failure detected. Terminating now.\n");
    EXIT_RET = EXIT_CODE_INIT_FAILURE;
    terminate = 1;
}
static void dfltSigHandler(int signum) {
    FlightComputer::stopSimulatedCycle();
}

int main(int argc, char* argv[]) {
    Os::Console::init();
    U32 uplink_port = 0; // Invalid port number forced
    U32 downlink_port = 0; // Invalid port number forced
    I32 option;
    char* hostname;
    option = 0;
    hostname = nullptr;

    // Check for environment variables
    const char* env_uplink_port = getenv("UPLINK_TARGET_PORT");
    const char* env_downlink_port = getenv("DOWNLINK_TARGET_PORT");
    const char* env_hostname = getenv("DOWNLINK_HOST");

    if (env_uplink_port) {
        uplink_port = static_cast<U32>(atoi(env_uplink_port));
    }
    if (env_downlink_port) {
        downlink_port = static_cast<U32>(atoi(env_downlink_port));
    }
    if (env_hostname) {
        hostname = strdup(env_hostname); // Use strdup to allocate memory for hostname
    }

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"downlink", required_argument, 0, 'd'},
        {"uplink", required_argument, 0, 'u'},
        {"address", required_argument, 0, 'a'},
        {"persist", no_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    while ((option = getopt_long(argc, argv, "hd:u:a:p", long_options, &option_index)) != -1) {
        switch(option) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'p':
                persist = true;
                break;
            case 'd':
                downlink_port = static_cast<U32>(atoi(optarg));
                break;
            case 'u':
                uplink_port = static_cast<U32>(atoi(optarg));
                break;
            case 'a':
                hostname = optarg;
                break;
            case '?':
            default:
                EXIT_RET = EXIT_CODE_STARTUP_FAILURE;
        }
    }

    // Check if required variables are set
    if (EXIT_RET != EXIT_CODE_OK || !hostname || uplink_port == 0 || downlink_port == 0) {
        fprintf(stderr, "Missing required parameters. Please provide all required options.\n");
        print_usage(argv[0]);
        return EXIT_RET;
    }

    (void) printf("Hit Ctrl-C to quit\n");

    // register signal handlers to exit program
    signal(SIGINT, dfltSigHandler);
    signal(SIGTERM, dfltSigHandler);
    signal(SIGUSR1, initFailureSigHandler);

    Fw::Logger::log("Main Starting init\n");
    FlightComputer::TopologyState state(hostname, uplink_port, downlink_port);
    (void) printf("Setting up sw runtime\n");
    FlightComputer::setupTopology(state);
    FlightComputer::startSimulatedCycle(Fw::TimeInterval(1, 0));  // Program loop cycling rate groups at 1Hz
    FlightComputer::teardownTopology(state);

    // Give time for threads to exit
    (void) printf("Waiting for threads...\n");
    Os::Task::delay(Fw::TimeInterval(1, 0));

    (void) printf("Exiting...\n");

    return EXIT_RET;
}
