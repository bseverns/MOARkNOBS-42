Import("env")

from platformio.test.result import TestSuite
from platformio.test.runners.factory import TestRunnerFactory


def configure_native_persistence_test(target_env):
    test_name = target_env.get("PIOTEST_RUNNING_NAME", "native_persistence")
    target_env.Append(
        CPPDEFINES=["UNIT_TEST"],
        PIOTEST_SRC_FILTER=["-<*>", "+<native_persistence/>"],
    )
    target_env.Prepend(
        CPPPATH=[
            "$PROJECT_TEST_DIR",
            "$PROJECT_TEST_DIR/native_persistence",
        ]
    )
    runner = TestRunnerFactory.new(
        TestSuite(target_env["PIOENV"], test_name),
        target_env.GetProjectConfig(),
    )
    runner.configure_build_env(target_env)
    target_env.Prepend(CPPPATH=["$PROJECT_TEST_DIR/native_persistence"])


env.AddMethod(configure_native_persistence_test, "ConfigureTestTarget")
