#!/usr/bin/env python
#
# Python-bindings open close testing program
#
# Copyright (C) 2010-2014, Joachim Metz <joachim.metz@gmail.com>
#
# Refer to AUTHORS for acknowledgements.
#
# This software is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this software.  If not, see <http://www.gnu.org/licenses/>.
#

import argparse
import sys

import pysmraw


def get_mode_string(mode):
  """Retrieves a human readable string representation of the access mode."""
  if mode == "r":
    mode_string = "read"
  elif mode == "w":
    mode_string = "write"
  else:
    mode_string = "unknown ({0:s})".format(mode)
  return mode_string


def pysmraw_test_single_open_close_file(filename, mode):
  if not filename:
    filename_string = "None"
  else:
    filename_string = filename

  print("Testing single open close of: {0:s} with access: {1:s}\t".format(
      filename_string, get_mode_string(mode)))

  result = True
  try:
    filenames = pysmraw.glob(filename)
    smraw_handle = pysmraw.handle()

    smraw_handle.open(filenames, mode)
    smraw_handle.close()

  except TypeError as exception:
    expected_message = (
        "{0:s}: unsupported string object type.").format(
            "pysmraw_handle_open")

    if not filename and str(exception) == expected_message:
      pass

    else:
      print(str(exception))
      result = False

  except ValueError as exception:
    expected_message = (
        "{0:s}: unsupported mode: w.").format(
            "pysmraw_handle_open")

    if mode != "w" or str(exception) != expected_message:
      print(str(exception))
      result = False

  except Exception as exception:
    print(str(exception))
    result = False

  if not result:
    print("(FAIL)")
  else:
    print("(PASS)")
  return result


def pysmraw_test_multi_open_close_file(filename, mode):
  print("Testing multi open close of: {0:s} with access: {1:s}\t".format(
      filename, get_mode_string(mode)))

  result = True
  try:
    filenames = pysmraw.glob(filename)
    smraw_handle = pysmraw.handle()

    smraw_handle.open(filenames, mode)
    smraw_handle.close()
    smraw_handle.open(filenames, mode)
    smraw_handle.close()

  except Exception as exception:
    print(str(exception))
    result = False

  if not result:
    print("(FAIL)")
  else:
    print("(PASS)")
  return result


def pysmraw_test_single_open_close_file_object(filename, mode):
  print(("Testing single open close of file-like object of: {0:s} "
         "with access: {1:s}\t").format(filename, get_mode_string(mode)))

  result = True
  try:
    filenames = pysmraw.glob(filename)
    file_objects = []
    for filename in filenames:
      file_object = open(filename, "rb")
      file_objects.append(file_object)

    smraw_handle = pysmraw.handle()

    smraw_handle.open_file_object(file_objects, mode)
    smraw_handle.close()

  except Exception as exception:
    print(str(exception))
    result = False

  if not result:
    print("(FAIL)")
  else:
    print("(PASS)")
  return result


def pysmraw_test_single_open_close_file_object_with_dereference(
    filename, mode):
  print(("Testing single open close of file-like object with dereference "
         "of: {0:s} with access: {1:s}\t").format(
      filename, get_mode_string(mode)))

  result = True
  try:
    filenames = pysmraw.glob(filename)
    file_objects = []
    for filename in filenames:
      file_object = open(filename, "rb")
      file_objects.append(file_object)

    smraw_handle = pysmraw.handle()

    smraw_handle.open_file_object(file_objects, mode)

    for file_object in file_objects:
      del file_object
    del file_objects

    smraw_handle.close()

  except Exception as exception:
    print(str(exception))
    result = False

  if not result:
    print("(FAIL)")
  else:
    print("(PASS)")
  return result


def pysmraw_test_multi_open_close_file_object(filename, mode):
  print(("Testing multi open close of file-like object of: {0:s} "
         "with access: {1:s}\t").format(filename, get_mode_string(mode)))

  result = True
  try:
    filenames = pysmraw.glob(filename)
    file_objects = []
    for filename in filenames:
      file_object = open(filename, "rb")
      file_objects.append(file_object)

    smraw_handle = pysmraw.handle()

    smraw_handle.open_file_object(file_objects, mode)
    smraw_handle.close()
    smraw_handle.open_file_object(file_objects, mode)
    smraw_handle.close()

  except Exception as exception:
    print(str(exception))
    result = False

  if not result:
    print("(FAIL)")
  else:
    print("(PASS)")
  return result


def main():
  args_parser = argparse.ArgumentParser(description=(
      "Tests open and close."))

  args_parser.add_argument(
      "source", nargs="?", action="store", metavar="FILENAME",
      default=None, help="The source filename.")

  options = args_parser.parse_args()

  if not options.source:
    print("Source value is missing.")
    print("")
    args_parser.print_help()
    print("")
    return False

  if not pysmraw_test_single_open_close_file(options.source, "r"):
    return False

  if not pysmraw_test_single_open_close_file(None, "r"):
    return False

  if not pysmraw_test_multi_open_close_file(options.source, "r"):
    return False

  if not pysmraw_test_single_open_close_file_object(options.source, "r"):
    return False

  if not pysmraw_test_single_open_close_file_object_with_dereference(
      options.source, "r"):
    return False

  if not pysmraw_test_multi_open_close_file_object(
      options.source, "r"):
    return False

  return True


if __name__ == "__main__":
  if not main():
    sys.exit(1)
  else:
    sys.exit(0)

