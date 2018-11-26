from setuptools import setup, Extension, find_packages
from typing import List
import os
import pip._internal

link_flags: List[str] = []
compile_flags: List[str] = ['-Ofast', '-std=c++14']


try:
    from Cython.Build import cythonize
except BaseException:
    pip._internal.main(['install', 'cython'])
    from Cython.Build import cythonize

ext_modules = [
    Extension(
        'hfst_lookup',
        ['hfst_lookup.pyx',
         'HfstFlagDiacritics.cc',
         'HfstExceptionDefs.cc',
         'transducer.cc',
         'HfstSymbolDefs.cc'],
        include_dirs=['.'],
        extra_link_args=link_flags,
        extra_compile_args=compile_flags,
        language='c++'
    ),
]

setup(
    name="hfst_lookup",
    version='0.0.1',
    packages=find_packages(),
    long_description='',
    classifiers=[],
    install_requires=[
        'cython'
    ],
    ext_modules=cythonize(ext_modules),
)
