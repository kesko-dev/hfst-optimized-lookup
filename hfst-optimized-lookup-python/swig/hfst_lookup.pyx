from libcpp.string cimport string
from libcpp.vector cimport vector

cdef extern from "transducer.h" namespace 'hfst_ol':
    cdef cppclass Transducer:
        Transducer(const string filename) except +
        vector[string] multi_lookup(vector[string] input_file)
        void write_lookup_cache()


cdef class PyTransducer:
    cdef Transducer *t
    def __cinit__(self):
        self.t = new Transducer('morphology.finntreebank.hfstol')

    def __dealloc__(self):
        del self.t

    def multi_lookup(self, list_of_strings):
        cdef vector[string] retvals
        list_of_bytes = [string.encode() for string in list_of_strings]
        retvals = self.t.multi_lookup(list_of_bytes)
        retval_py = list()
        for val in retvals:
            retval_py.append(val.decode())
        return retval_py
