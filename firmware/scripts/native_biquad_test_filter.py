Import("env")

from platformio.test.result import TestSuite
from platformio.test.runners.factory import TestRunnerFactory


# PlatformIO deliberately adds loose files in test/ as shared support to every
# suite. This host-only DSP lane must instead build only its own directory:
# those shared files are Teensy/Arduino fixtures and are outside this test's
# hardware-free contract. ConfigureTestTarget normally adds +<*.cpp>; replace
# that method before the project library builder invokes it.
def configure_native_biquad_test(target_env):
    test_name = target_env.get("PIOTEST_RUNNING_NAME", "native_biquad")
    target_env.Append(
        CPPDEFINES=["UNIT_TEST"],
        PIOTEST_SRC_FILTER=["-<*>", "+<native_biquad/>"],
    )
    target_env.Prepend(
        CPPPATH=[
            "$PROJECT_TEST_DIR",
            "$PROJECT_TEST_DIR/native_biquad",
        ]
    )
    runner = TestRunnerFactory.new(
        TestSuite(target_env["PIOENV"], test_name),
        target_env.GetProjectConfig(),
    )
    runner.configure_build_env(target_env)


env.AddMethod(configure_native_biquad_test, "ConfigureTestTarget")
