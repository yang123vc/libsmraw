/*
 * Storage media (SM) RAW library read testing program
 *
 * Copyright (c) 2010-2012, Joachim Metz <joachim.metz@gmail.com>
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <file_stream.h>

#if defined( HAVE_STDLIB_H ) || defined( WINAPI )
#include <stdlib.h>
#endif

#include <signal.h>

#include "smraw_test_libcstring.h"
#include "smraw_test_libsmraw.h"

#define SMRAW_TEST_READ_BUFFER_SIZE	4096

int smraw_test_read_signal = 0;

void smraw_test_read_signal_handler(
      int signal )
{
	smraw_test_read_signal = 1;
}

/* Tests libsmraw_seek_offset and libsmraw_handle_read_buffer
 * Returns 1 if successful, 0 if not or -1 on error
 */
int smraw_test_read_buffer(
     libsmraw_handle_t *handle,
     off64_t input_offset,
     int input_whence,
     size64_t input_size,
     off64_t output_offset,
     size64_t output_size )
{
	uint8_t buffer[ SMRAW_TEST_READ_BUFFER_SIZE ];

	libsmraw_error_t *error   = NULL;
	const char *whence_string = NULL;
	off64_t result_offset     = 0;
	size64_t remaining_size   = 0;
	size64_t result_size      = 0;
	size_t read_size          = 0;
	ssize_t read_count        = 0;
	int result                = 0;

	if( handle == NULL )
	{
		return( -1 );
	}
	if( input_whence == SEEK_CUR )
	{
		whence_string = "SEEK_CUR";
	}
	else if( input_whence == SEEK_END )
	{
		whence_string = "SEEK_END";
	}
	else if( input_whence == SEEK_SET )
	{
		whence_string = "SEEK_SET";
	}
	else
	{
		whence_string = "UNKNOWN";
	}
	fprintf(
	 stdout,
	 "Testing reading range with offset: %" PRIi64 ", whence: %s and size: %" PRIu64 "\t",
	 input_offset,
	 whence_string,
	 input_size );

	result_offset = libsmraw_handle_seek_offset(
	                 handle,
	                 input_offset,
	                 input_whence,
	                 &error );

	if( result_offset == -1 )
	{
		libsmraw_error_backtrace_fprint(
		 error,
		 stderr );

		libsmraw_error_free(
		 &error );
	}
	if( result_offset != output_offset )
	{
		fprintf(
		 stderr,
		 "Unexpected result offset: %" PRIi64 "\n",
		 result_offset );
	}
	else if( result_offset == -1 )
	{
		result = 1;
	}
	else if( result_offset >= 0 )
	{
		remaining_size = input_size;

		while( remaining_size > 0 )
		{
			read_size = SMRAW_TEST_READ_BUFFER_SIZE;

			if( remaining_size < (size64_t) read_size )
			{
				read_size = (size_t) remaining_size;
			}
			read_count = libsmraw_handle_read_buffer(
				      handle,
				      buffer,
				      read_size,
				      &error );

			if( read_count < 0 )
			{
				libsmraw_error_backtrace_fprint(
				 error,
				 stderr );

				libsmraw_error_free(
				 &error );

				break;
			}
			remaining_size -= (size64_t) read_count;
			result_size    += (size64_t) read_count;

			if( read_count != (ssize_t) read_size )
			{
				break;
			}
		}
		if( output_size == result_size )
		{
			result = 1;
		}
		else
		{
			fprintf(
			 stderr,
			 "Unexpected read count: %" PRIu64 "\n",
			 result_size );
		}
	}
	else
	{
		fprintf(
		 stderr,
		 "Unexpected result offset: %" PRIi64 "\n",
		 result_offset );
	}
	if( result != 0 )
	{
		fprintf(
		 stdout,
		 "(PASS)" );
	}
	else
	{
		fprintf(
		 stdout,
		 "(FAIL)" );
	}
	fprintf(
	 stdout,
	 "\n" );

	return( result );
}

/* The main program
 */
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
int wmain( int argc, wchar_t * const argv[] )
#else
int main( int argc, char * const argv[] )
#endif
{
	libsmraw_error_t *error   = NULL;
	libsmraw_handle_t *handle = NULL;
	size64_t media_size       = 0;
	size64_t read_size        = 0;

	if( argc < 2 )
	{
		fprintf(
		 stderr,
		 "Missing filename(s).\n" );

		return( EXIT_FAILURE );
	}
	/* Initialization
	 */
	if( libsmraw_handle_initialize(
	     &handle,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to create handle.\n" );

		goto on_error;
	}
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
	if( libsmraw_handle_open_wide(
	     handle,
	     &( argv[ 1 ] ),
	     argc - 1,
	     LIBSMRAW_OPEN_READ,
	     &error ) != 1 )
#else
	if( libsmraw_handle_open(
	     handle,
	     &( argv[ 1 ] ),
	     argc - 1,
	     LIBSMRAW_OPEN_READ,
	     &error ) != 1 )
#endif
	{
		fprintf(
		 stderr,
		 "Unable to open file(s).\n" );

		goto on_error;
	}
	if( libsmraw_handle_get_media_size(
	     handle,
	     &media_size,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to retrieve media size.\n" );

		goto on_error;
	}
	if( media_size > (size64_t) INT64_MAX )
	{
		fprintf(
		 stderr,
		 "Media size exceeds maximum.\n" );

		goto on_error;
	}
	fprintf(
	 stdout,
	 "Media size: %" PRIu64 " bytes\n",
	 media_size );

	signal(
	 SIGINT,
	 &smraw_test_read_signal_handler );

	while( smraw_test_read_signal == 0 )
	{
		read_size = random();

		if( read_size > media_size )
		{
			read_size %= media_size;
		}
		else
		{
			read_size = media_size % read_size;
		}
		/* Test: offset: 0 size: <read_size>
		 * Expected result: offset: 0 size: <read_size>
		 */
		if( smraw_test_read_buffer(
		     handle,
		     0,
		     SEEK_SET,
		     read_size,
		     0,
		     read_size ) != 1 )
		{
			fprintf(
			 stderr,
			 "Unable to test read buffer.\n" );

			goto on_error;
		}
		/* Test: offset: 0 size: <read_size>
		 * Expected result: ffset: 0 size: <read_size>
		 */
		if( smraw_test_read_buffer(
		     handle,
		     0,
		     SEEK_SET,
		     read_size,
		     0,
		     read_size ) != 1 )
		{
			fprintf(
			 stderr,
			 "Unable to test read buffer.\n" );

			goto on_error;
		}
		/* Test: offset: <read_size / 7> size: <read_size / 2>
		 * Expected result: offset: <read_size / 7> size: <read_size / 2>
		 */
		if( smraw_test_read_buffer(
		     handle,
		     (off64_t) ( read_size / 7 ),
		     SEEK_SET,
		     read_size / 2,
		     (off64_t) ( read_size / 7 ),
		     read_size / 2 ) != 1 )
		{
			fprintf(
			 stderr,
			 "Unable to test read buffer.\n" );

			goto on_error;
		}
		/* Test: offset: <read_size / 7> size: <read_size / 2>
		 * Expected result: offset: <read_size / 7> size: <read_size / 2>
		 */
		if( smraw_test_read_buffer(
		     handle,
		     (off64_t) ( read_size / 7 ),
		     SEEK_SET,
		     read_size / 2,
		     (off64_t) ( read_size / 7 ),
		     read_size / 2 ) != 1 )
		{
			fprintf(
			 stderr,
			 "Unable to test read buffer.\n" );

			goto on_error;
		}
		if( ( media_size - read_size ) < 4096 )
		{
			/* Test: offset: <read_size> size: 4096
			 * Expected result: offset: <read_size> size: <4096 - media_size - read_size>
			 */
			if( smraw_test_read_buffer(
			     handle,
			     (off64_t) read_size,
			     SEEK_SET,
			     4096,
			     (off64_t) read_size,
			     4096 - ( media_size - read_size ) ) != 1 )
			{
				fprintf(
				 stderr,
				 "Unable to test read buffer.\n" );

				goto on_error;
			}
			/* Test: offset: <read_size> size: 4096
			 * Expected result: offset: <read_size> size: <4096 - media_size - read_size>
			 */
			if( smraw_test_read_buffer(
			     handle,
			     (off64_t) read_size,
			     SEEK_SET,
			     4096,
			     (off64_t) read_size,
			     4096 - ( media_size - read_size ) ) != 1 )
			{
				fprintf(
				 stderr,
				 "Unable to test read buffer.\n" );

				goto on_error;
			}
		}
		else
		{
			/* Test: offset: <read_size> size: 4096
			 * Expected result: offset: <read_size> size: 4096
			 */
			if( smraw_test_read_buffer(
			     handle,
			     (off64_t) read_size,
			     SEEK_SET,
			     4096,
			     (off64_t) read_size,
			     4096 ) != 1 )
			{
				fprintf(
				 stderr,
				 "Unable to test read buffer.\n" );

				goto on_error;
			}
			/* Test: offset: <read_size> size: 4096
			 * Expected result: offset: <read_size> size: 4096
			 */
			if( smraw_test_read_buffer(
			     handle,
			     (off64_t) read_size,
			     SEEK_SET,
			     4096,
			     (off64_t) read_size,
			     4096 ) != 1 )
			{
				fprintf(
				 stderr,
				 "Unable to test read buffer.\n" );

				goto on_error;
			}
		}
	}
	signal(
	 SIGINT,
	 SIG_DFL );

	/* Clean up
	 */
	if( libsmraw_handle_close(
	     handle,
	     &error ) != 0 )
	{
		fprintf(
		 stderr,
		 "Unable to close file(s).\n" );

		goto on_error;
	}
	if( libsmraw_handle_free(
	     &handle,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to free handle.\n" );

		goto on_error;
	}
	return( EXIT_SUCCESS );

on_error:
	if( error != NULL )
	{
		libsmraw_error_backtrace_fprint(
		 error,
		 stderr );
		libsmraw_error_free(
		 &error );
	}
	if( handle != NULL )
	{
		libsmraw_handle_close(
		 handle,
		 NULL );
		libsmraw_handle_free(
		 &handle,
		 NULL );
	}
	return( EXIT_FAILURE );
}

