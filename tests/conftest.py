import pickle


def pytest_generate_tests(metafunc):
    if 'pickle_protocol' in metafunc.fixturenames:
        metafunc.parametrize("pickle_protocol",
                             list(range(pickle.HIGHEST_PROTOCOL)),
                             scope='session')
