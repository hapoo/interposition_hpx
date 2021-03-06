//  Copyright (c) 2017 Alireza Kheirkhahan
//  Copyright (c) 2016 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/process.hpp>
#include <hpx/util/lightweight_test.hpp>

#include <hpxio/server/local_file.hpp>
#include <hpxio/base_file.hpp>

#include <boost/filesystem.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
inline int get_arraylen(char **arr)
{
    int count = 0;
    if (nullptr != arr) {
        while(nullptr != arr[count])
            ++count;   // simply count the strings
    }
    return count;
}

std::vector<std::string> get_environment()
{
    std::vector<std::string> env;
#if defined(HPX_WINDOWS)
    int len = get_arraylen(_environ);
    std::copy(&_environ[0], &_environ[len], std::back_inserter(env));
#elif defined(linux) || defined(__linux) || defined(__linux__) || \
      defined(__AIX__) || defined(__APPLE__)
    int len = get_arraylen(environ);
    std::copy(&environ[0], &environ[len], std::back_inserter(env));
#else
#error "Don't know, how to access the execution environment on this platform"
#endif
    return env;
}

// ----------------------------------------------------------------------------
int hpx_main(boost::program_options::variables_map &vm)
{
    namespace process = hpx::components::process;
    namespace fs = boost::filesystem;

    // find where the HPX core libraries are located
    fs::path base_dir = hpx::util::find_prefix();
    base_dir /= "bin";

    fs::path exe = base_dir / "launched_process_test" HPX_EXECUTABLE_EXTENSION;

    std::string launch_target;
    if (vm.count("launch"))
    {
        launch_target = vm["launch"].as<std::string>();
        std::cout << "using launch: " << launch_target << std::endl;
        exe = launch_target;
    }
    else
    {
        std::cout << "using launch (default): " << exe << std::endl;
    }

    // set up command line for launched executable
    std::vector<std::string> args;
    args.push_back(exe.string());
//    args.push_back("--exit_code=42");
    args.push_back("--component=test_file");
//    args.push_back("--set_message=accessed");
//    args.push_back("--hpx:ignore-batch-env");
//    args.push_back("--hpx:attach-debugger=startup");

    // set up environment for launched executable
    std::vector<std::string> env = get_environment();   // current environment

    // Pass along the console parcelport address
    env.push_back("HPX_AGAS_SERVER_ADDRESS=" +
                  hpx::get_config_entry("hpx.agas.address", HPX_INITIAL_IP_ADDRESS));
    env.push_back("HPX_AGAS_SERVER_PORT=" +
                  hpx::get_config_entry("hpx.agas.port",
                                        std::to_string(HPX_INITIAL_IP_PORT)));

    // Pass along the parcelport address which should be used by the launched
    // executable

    // The launched executable will run on the same host as this test
    int port = 42;  // each launched HPX locality needs to be assigned a
    // unique port

    env.push_back("HPX_PARCEL_SERVER_ADDRESS=" +
                  hpx::get_config_entry("hpx.agas.address", HPX_INITIAL_IP_ADDRESS));
    env.push_back("HPX_PARCEL_SERVER_PORT=" +
                  std::to_string(HPX_CONNECTING_IP_PORT - port));

    // Instruct new locality to connect back on startup using the given name.
    env.push_back("HPX_ON_STARTUP_WAIT_ON_LATCH=launch_process");
    env.push_back("LD_PRELOAD=/home/alireza/src/dev/interposition_hpx/cmake-build-debug/lib/libhpx_interposition.so");

    // launch test executable
    process::child c = process::execute(
            hpx::find_here(),
            process::run_exe(exe.string()),
            process::set_args(args),
            process::set_env(env),
            process::start_in_dir(base_dir.string()),
            process::throw_on_error(),
            process::wait_on_latch("launch_process")   // same as above!
    );


    {
        hpx::io::base_file f =
                hpx::new_<hpx::io::server::local_file>(hpx::find_here());

        f.open(hpx::launch::sync, "test.txt", O_WRONLY | O_APPEND | O_CREAT);

        // register the component instance with AGAS
        f.register_as("test_file");       // same as --component=<> above

        // the launched executable should have connected back as a new locality
//        HPX_TEST_EQ(hpx::find_all_localities().size(), std::size_t(2));

        // wait for it to exit, we know it returns 42 (see --exit_code=<> above)
        int exit_code = c.wait_for_exit(hpx::launch::sync);
        HPX_TEST_EQ(exit_code, 42);

    }  // release the component

    // the new locality should have disconnected now
    HPX_TEST_EQ(hpx::find_all_localities().size(), std::size_t(1));

    return hpx::finalize();
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    // add command line option which controls the random number generator seed
    using namespace boost::program_options;
    options_description desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

    desc_commandline.add_options()("launch,l", value<std::string>(),
                                   "the process that will be launched and which connects back");

    // This explicitly enables the component we depend on (it is disabled by
    // default to avoid being loaded outside of this test).
    std::vector<std::string> const cfg = {
            "hpx.components.launch_process_test_server.enabled!=1"
    };

    HPX_TEST_EQ_MSG(
            hpx::init(desc_commandline, argc, argv, cfg), 0,
            "HPX main exited with non-zero status"
    );

    return hpx::util::report_errors();
}
