from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

setup(
    ext_modules = cythonize([
        Extension('tobii_client', ['tobii_client.pyx'],
            include_dirs=['include'],
            libraries=['tobii_stream_engine'],
        )
    ], language_level=3)
)