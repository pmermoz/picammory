//
//    PiCammory
//    Copyright (C) 2014 Pascal Mermoz <pascal@mermoz.net>.
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//  cservice.c
//  
//
//  Created by dev on 4/7/14.
//
//

// <!> Python.h must be the first import ( https://docs.python.org/3.2/extending/extending.html )
#include <Python.h>
//-----------------

#include <stdio.h>

#include "cservice.h"
#include "motionservice.h"

//------------------------------------------------------------------------------------------------------------------------------------
//---
//------------------------------------------------------------------------------------------------------------------------------------

static Py_buffer debugImageBuffer;

// Memory leak if py_setup_motion is called multiple time, without py_cleanup_motion between...
static PyObject *py_setup_motion(PyObject *self, PyObject *args) {
  int imageWidth;
  int imageHeight;
  int trace = False;
  double result;

  memset(&debugImageBuffer, 0, sizeof(debugImageBuffer));

  /* Get the passed Python object */
  // (w,h)  Image Size
  // Optional:
  // Trace
  if (!PyArg_ParseTuple(args, "(ii)|y*i", &imageWidth, &imageHeight, &debugImageBuffer, &trace)) {
   return NULL;
  }

  /* Pass the raw buffer and size to the C function */
  result = setup_motion(imageWidth, imageHeight, debugImageBuffer.buf, trace);

  /* Indicate we're done working with the buffer */
  return Py_BuildValue("d", result);
}

static PyObject *py_cleanup_motion(PyObject *self, PyObject *args) {
    double result;

    /* Pass the raw buffer and size to the C function */
    result = cleanup_motion();

    PyBuffer_Release(&debugImageBuffer);

    return Py_BuildValue("d", result);
}

static PyObject *py_detect_motion(PyObject *self, PyObject *args) {
  Py_buffer view1;
  Py_buffer view2;
  double result;
  int trace = False;

  /* Get the passed Python object */
  //	if (!PyArg_ParseTuple(args, "dd:addf", &a, &b)) {
  if (!PyArg_ParseTuple(args, "y*y*|i", &view1, &view2, &trace)) {
   return NULL;
  }

  if (trace) {
      printf("view.buf= (0x%x, 0x%x)\n", view1.buf, view2.buf);
  }

  if (view1.ndim != 1) {
    PyErr_SetString(PyExc_TypeError, "Expected a 1-dimensional array");
    PyBuffer_Release(&view1);
    PyBuffer_Release(&view2);
    return NULL;
  }
  if (view2.ndim != 1) {
    PyErr_SetString(PyExc_TypeError, "Expected a 1-dimensional array");
    PyBuffer_Release(&view1);
    PyBuffer_Release(&view2);
    return NULL;
  }

  /* Pass the raw buffer and size to the C function */
  result = detect_motion(view1.buf, view2.buf, trace);

  /* Indicate we're done working with the buffer */
  PyBuffer_Release(&view1);
  PyBuffer_Release(&view2);
  return Py_BuildValue("d", result);
}


static PyObject *py_motion_message(PyObject *self, PyObject *args) {
    const char* result = motionMessage();
    return Py_BuildValue("s", result);
}

static PyObject *py_write_image_buffer(PyObject *self, PyObject *args) {
  char* filename;
  double result = 0;
  int trace = False;

  /* Get the passed Python object */
  if (!PyArg_ParseTuple(args, "s", &filename)) {
   return NULL;
  }

  /* Pass the raw buffer and size to the C function */
  imageBufferWriteBuffer(filename);

  /* Indicate we're done working with the buffer */
  return Py_BuildValue("d", result);
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

/* Module method table */
static PyMethodDef CServiceMethods[] = {
  {"setup_motion",  py_setup_motion, METH_VARARGS, "Setup motion detection environment"},
  {"cleanup_motion",  py_cleanup_motion, METH_VARARGS, "Cleanup motion detection environment"},
  {"detect_motion",  py_detect_motion, METH_VARARGS, "Detect motion between 2 buffers image"},
  {"motion_message",  py_motion_message, METH_VARARGS, "Message generated during motion detection"},
  {"write_image_buffer",  py_write_image_buffer, METH_VARARGS, ""},
  { NULL, NULL, 0, NULL}
};

/* Module structure */
static struct PyModuleDef cservicemodule = {
  PyModuleDef_HEAD_INIT,
  "cservice",           /* name of module */
  "C Services",  /* Doc string (may be NULL) */
  -1,                 /* Size of per-interpreter state or -1 */
  CServiceMethods       /* Method table */
};

/* Module initialization function */
PyMODINIT_FUNC
PyInit_cservice(void) {
  return PyModule_Create(&cservicemodule);
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
