#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include <assert.h>
#include <emscripten.h>
#include <stdalign.h>

#include "error_handling.h"
#include "hiwire.h"
#include "js2python.h"
#include "jsimport.h"
#include "jsproxy.h"
#include "pyproxy.h"
#include "python2js.h"
#include "runpython.h"

#define FATAL_ERROR(args...)                                                   \
  do {                                                                         \
    printf("FATAL ERROR: ");                                                   \
    printf(args);                                                              \
    if (PyErr_Occurred()) {                                                    \
      printf("Error was triggered by Python exception:\n");                    \
      PyErr_Print();                                                           \
    }                                                                          \
    return -1;                                                                 \
  } while (0)

#define TRY_INIT(mod)                                                          \
  do {                                                                         \
    if (mod##_init()) {                                                        \
      FATAL_ERROR("Failed to initialize module %s.\n", #mod);                  \
    }                                                                          \
  } while (0)

#define TRY_INIT_WITH_CORE_MODULE(mod)                                         \
  do {                                                                         \
    if (mod##_init(core_module)) {                                             \
      FATAL_ERROR("Failed to initialize module %s.\n", #mod);                  \
    }                                                                          \
  } while (0)

static struct PyModuleDef core_module_def = {
  PyModuleDef_HEAD_INIT,
  .m_name = "_pyodide_core",
  .m_doc = "Pyodide C builtins",
  .m_size = -1,
};

int
main(int argc, char** argv)
{
  PyObject* sys = NULL;
  PyObject* core_module = NULL;
  TRY_INIT(hiwire);

  setenv("PYTHONHOME", "/", 0);
  // This doesn't seem to work anymore, but I'm keeping it for good measure
  // anyway The effective way to turn this off is below: setting
  // sys.dont_write_bytecode = True
  setenv("PYTHONDONTWRITEBYTECODE", "1", 0);

  Py_InitializeEx(0);

  sys = PyImport_ImportModule("sys");
  if (sys == NULL) {
    FATAL_ERROR("Failed to import sys module.");
  }

  if (PyObject_SetAttrString(sys, "dont_write_bytecode", Py_True)) {
    FATAL_ERROR("Failed to set attribute on sys module.");
  }

  if (alignof(JsRef) != alignof(int)) {
    FATAL_ERROR("JsRef doesn't have the same alignment as int.");
  }

  if (sizeof(JsRef) != sizeof(int)) {
    FATAL_ERROR("JsRef doesn't have the same size as int.");
  }

  core_module = PyModule_Create(&core_module_def);
  if (core_module == NULL) {
    FATAL_ERROR("Failed to create core module.");
  }

  TRY_INIT(error_handling);
  TRY_INIT(js2python);
  TRY_INIT(JsImport);
  TRY_INIT_WITH_CORE_MODULE(JsProxy);
  TRY_INIT(pyproxy);
  TRY_INIT(python2js);

  PyObject* module_dict = PyImport_GetModuleDict(); // borrowed
  if (PyDict_SetItemString(module_dict, "_pyodide_core", core_module)) {
    FATAL_ERROR("Failed to add '_pyodide_core' module to modules dict.");
  }

  // pyodide.py imported for these two.
  // They should appear last so that core_module is ready.
  TRY_INIT(runpython);

  Py_CLEAR(sys);
  Py_CLEAR(core_module);
  printf("Python initialization complete\n");
  emscripten_exit_with_live_runtime();
  return 0;
}