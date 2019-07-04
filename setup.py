from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

setup(
    name='tobii_client',
    description='Python wrapper for Tobii Stream API',
    ext_modules = cythonize([
        Extension('tobii_client', ['tobii_client.pyx'],
            include_dirs=['include'],
            libraries=['tobii_stream_engine'],
        )
    ], language_level=3),
    data_files=[('', ['tobii_stream_engine.dll', 'tobii_stream_engine.lib'])]
)