/*
 * Mounts a storage media (split) RAW image file
 *
 * Copyright (C) 2010-2012, Joachim Metz <jbmetz@users.sourceforge.net>
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
#include <memory.h>
#include <types.h>

#include <libcstring.h>
#include <liberror.h>

#if defined( HAVE_ERRNO_H )
#include <errno.h>
#endif

#if defined( HAVE_UNISTD_H )
#include <unistd.h>
#endif

#if defined( HAVE_STDLIB_H ) || defined( WINAPI )
#include <stdlib.h>
#endif

#include <libsystem.h>

#if defined( HAVE_FUSE_H ) || defined( HAVE_OSXFUSE_FUSE_H )
#define FUSE_USE_VERSION        26

#if defined( HAVE_FUSE_H )
#include <fuse.h>

#elif defined( HAVE_OSXFUSE_FUSE_H )
#include <osxfuse/fuse.h>
#endif

#endif /* defined( HAVE_FUSE_H ) || defined( HAVE_OSXFUSE_FUSE_H ) */

#include "mount_handle.h"
#include "smrawoutput.h"
#include "smrawtools_libsmraw.h"

mount_handle_t *smrawmount_mount_handle = NULL;
int smrawmount_abort                    = 0;

/* Prints the executable usage information
 */
void usage_fprint(
      FILE *stream )
{
	if( stream == NULL )
	{
		return;
	}
	fprintf( stream, "Use smrawmount to mount a storage media (split) RAW (image) file\n\n" );

	fprintf( stream, "Usage: smrawmount [ -hvV ] smraw_files mount_point\n\n" );

	fprintf( stream, "\tsmraw_files: the first or the entire set of SMRAW segment files\n\n" );
	fprintf( stream, "\tmount_point: the directory to serve as mount point\n\n" );

	fprintf( stream, "\t-h:          shows this help\n" );
	fprintf( stream, "\t-v:          verbose output to stderr\n"
	                 "\t             smrawmount will remain running in the foregroud\n" );
	fprintf( stream, "\t-V:          print version\n" );
}

/* Signal handler for smrawmount
 */
void smrawmount_signal_handler(
      libsystem_signal_t signal LIBSYSTEM_ATTRIBUTE_UNUSED )
{
	liberror_error_t *error = NULL;
	static char *function   = "smrawmount_signal_handler";

	LIBSYSTEM_UNREFERENCED_PARAMETER( signal )

	smrawmount_abort = 1;

	if( smrawmount_mount_handle != NULL )
	{
		if( mount_handle_signal_abort(
		     smrawmount_mount_handle,
		     &error ) != 1 )
		{
			libsystem_notify_printf(
			 "%s: unable to signal mount handle to abort.\n",
			 function );

			libsystem_notify_print_error_backtrace(
			 error );
			liberror_error_free(
			 &error );
		}
	}
	/* Force stdin to close otherwise any function reading it will remain blocked
	 */
	if( libsystem_file_io_close(
	     0 ) != 0 )
	{
		libsystem_notify_printf(
		 "%s: unable to close stdin.\n",
		 function );
	}
}

#if defined( HAVE_LIBFUSE )

#if ( SIZEOF_OFF_T != 8 ) && ( SIZEOF_OFF_T != 4 )
#error Size of off_t not supported
#endif

static char *smrawmount_fuse_path         = "/raw1";
static size_t smrawmount_fuse_path_length = 5;

/* Opens a file
 * Returns 0 if successful or a negative errno value otherwise
 */
int smrawmount_fuse_open(
     const char *path,
     struct fuse_file_info *file_info )
{
	liberror_error_t *error = NULL;
	static char *function   = "smrawmount_fuse_open";
	size_t path_length      = 0;
	int result              = 0;

	if( path == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( file_info == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file info.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( ( path_length != smrawmount_fuse_path_length )
	 || ( libcstring_narrow_string_compare(
	       path,
	       smrawmount_fuse_path,
	       smrawmount_fuse_path_length ) != 0 ) )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported path.",
		 function );

		result = -ENOENT;

		goto on_error;
	}
	if( ( file_info->flags & 0x03 ) != O_RDONLY )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
		 "%s: write access currently not supported.",
		 function );

		result = -EACCES;

		goto on_error;
	}
	return( 0 );

on_error:
	if( error != NULL )
	{
		libsystem_notify_print_error_backtrace(
		 error );
		liberror_error_free(
		 &error );
	}
	return( result );
}

/* Reads a buffer of data at the specified offset
 * Returns number of bytes read if successful or a negative errno value otherwise
 */
int smrawmount_fuse_read(
     const char *path,
     char *buffer,
     size_t size,
     off_t offset,
     struct fuse_file_info *file_info )
{
	liberror_error_t *error = NULL;
	static char *function   = "smrawmount_fuse_read";
	size_t path_length      = 0;
	ssize_t read_count      = 0;
	int result              = 0;

	if( path == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( size > (size_t) INT_MAX )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid size value exceeds maximum.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( file_info == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file info.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( ( path_length != smrawmount_fuse_path_length )
	 || ( libcstring_narrow_string_compare(
	       path,
	       smrawmount_fuse_path,
	       smrawmount_fuse_path_length ) != 0 ) )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported path.",
		 function );

		result = -ENOENT;

		goto on_error;
	}
	if( mount_handle_seek_offset(
	     smrawmount_mount_handle,
	     (off64_t) offset,
	     SEEK_SET,
	     &error ) == -1 )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_IO,
		 LIBERROR_IO_ERROR_SEEK_FAILED,
		 "%s: unable to seek offset in mount handle.",
		 function );

		result = -EIO;

		goto on_error;
	}
	read_count = mount_handle_read_buffer(
	              smrawmount_mount_handle,
	              (uint8_t *) buffer,
	              size,
	              &error );

	if( read_count == -1 )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_IO,
		 LIBERROR_IO_ERROR_READ_FAILED,
		 "%s: unable to read from mount handle.",
		 function );

		result = -EIO;

		goto on_error;
	}
	return( (int) read_count );

on_error:
	if( error != NULL )
	{
		libsystem_notify_print_error_backtrace(
		 error );
		liberror_error_free(
		 &error );
	}
	return( result );
}

/* Reads a directory
 * Returns 0 if successful or a negative errno value otherwise
 */
int smrawmount_fuse_readdir(
     const char *path,
     void *buffer,
     fuse_fill_dir_t filler,
     off_t offset LIBSYSTEM_ATTRIBUTE_UNUSED,
     struct fuse_file_info *file_info LIBSYSTEM_ATTRIBUTE_UNUSED )
{
	liberror_error_t *error = NULL;
	static char *function   = "smrawmount_fuse_readdir";
	size_t path_length      = 0;
	int result              = 0;

	LIBSYSTEM_UNREFERENCED_PARAMETER( offset )
	LIBSYSTEM_UNREFERENCED_PARAMETER( file_info )

	if( path == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( ( path_length != 1 )
	 || ( path[ 0 ] != '/' ) )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported path.",
		 function );

		result = -ENOENT;

		goto on_error;
	}
	if( filler(
	     buffer,
	     ".",
	     NULL,
	     0 ) == 1 )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set directory entry.",
		 function );

		result = -EIO;

		goto on_error;
	}
	if( filler(
	     buffer,
	     "..",
	     NULL,
	     0 ) == 1 )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set directory entry.",
		 function );

		result = -EIO;

		goto on_error;
	}
	if( filler(
	     buffer,
	     &( smrawmount_fuse_path[ 1 ] ),
	     NULL,
	     0 ) == 1 )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set directory entry.",
		 function );

		result = -EIO;

		goto on_error;
	}
	return( 0 );

on_error:
	if( error != NULL )
	{
		libsystem_notify_print_error_backtrace(
		 error );
		liberror_error_free(
		 &error );
	}
	return( result );
}

/* Retrieves the file stat info
 * Returns 0 if successful or a negative errno value otherwise
 */
int smrawmount_fuse_getattr(
     const char *path,
     struct stat *stat_info )
{
	liberror_error_t *error = NULL;
	static char *function   = "smrawmount_fuse_getattr";
	size64_t media_size     = 0;
	size_t path_length      = 0;
	int result              = -ENOENT;

	if( path == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( stat_info == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid stat info.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( memory_set(
	     stat_info,
	     0,
	     sizeof( struct stat ) ) == NULL )
	{
		liberror_error_set(
		 &error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear stat info.",
		 function );

		result = errno;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( path_length == 1 )
	{
		if( path[ 0 ] == '/' )
		{
			stat_info->st_mode  = S_IFDIR | 0755;
			stat_info->st_nlink = 2;

			result = 0;
		}
	}
	else if( path_length == smrawmount_fuse_path_length )
	{
		if( libcstring_narrow_string_compare(
		     path,
		     smrawmount_fuse_path,
		     smrawmount_fuse_path_length ) == 0 )
		{
			stat_info->st_mode  = S_IFREG | 0444;
			stat_info->st_nlink = 1;

			if( mount_handle_get_media_size(
			     smrawmount_mount_handle,
			     &media_size,
			     &error ) != 1 )
			{
				liberror_error_set(
				 &error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable to retrieve media size.",
				 function );

				result = -EBADFD;

				goto on_error;
			}
#if SIZEOF_OFF_T == 4
			if( media_size > (size64_t) UINT32_MAX )
			{
				liberror_error_set(
				 &error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid media size value out of bounds.",
				 function );

				result = -ERANGE;

				goto on_error;
			}
#endif
			stat_info->st_size = (off_t) media_size;

			result = 0;
		}
	}
	if( result == 0 )
	{
		stat_info->st_atime = libsystem_date_time_time(
		                       NULL );

		stat_info->st_mtime = libsystem_date_time_time(
		                       NULL );

		stat_info->st_ctime = libsystem_date_time_time(
		                       NULL );

#if defined( HAVE_GETEUID )
		stat_info->st_uid = geteuid();
#else
		stat_info->st_uid = 0;
#endif
#if defined( HAVE_GETEGID )
		stat_info->st_gid = getegid();
#else
		stat_info->st_gid = 0;
#endif
	}
	return( result );

on_error:
	if( error != NULL )
	{
		libsystem_notify_print_error_backtrace(
		 error );
		liberror_error_free(
		 &error );
	}
	return( result );
}

#endif /* defined( HAVE_LIBFUSE ) */

/* The main program
 */
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
int wmain( int argc, wchar_t * const argv[] )
#else
int main( int argc, char * const argv[] )
#endif
{
	libcstring_system_character_t * const *argv_filenames = NULL;

	libsmraw_error_t *error                               = NULL;
	libcstring_system_character_t *mount_point            = NULL;
	char *program                                         = "smrawmount";
	libcstring_system_integer_t option                    = 0;
	int number_of_filenames                               = 0;
	int verbose                                           = 0;

#if !defined( LIBSYSTEM_HAVE_GLOB )
	libsystem_glob_t *glob                                = NULL;
#endif

#if defined( HAVE_LIBFUSE )
	struct fuse_operations smrawmount_fuse_operations;
	struct fuse_chan *smrawmount_fuse_channel             = NULL;
	struct fuse *smrawmount_fuse_handle                   = NULL;
	int result                                            = 0;
#endif

	libsystem_notify_set_stream(
	 stderr,
	 NULL );
	libsystem_notify_set_verbose(
	 1 );

        if( libsystem_initialize(
             "smrawtools",
             _IONBF,
             &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to initialize system values.\n" );

		libsystem_notify_print_error_backtrace(
		 error );
		liberror_error_free(
		 &error );

		return( EXIT_FAILURE );
	}
	smrawoutput_version_fprint(
	 stdout,
	 program );

	while( ( option = libsystem_getopt(
	                   argc,
	                   argv,
	                   _LIBCSTRING_SYSTEM_STRING( "hvV" ) ) ) != (libcstring_system_integer_t) -1 )
	{
		switch( option )
		{
			case (libcstring_system_integer_t) '?':
			default:
				fprintf(
				 stderr,
				 "Invalid argument: %" PRIs_LIBCSTRING_SYSTEM "\n",
				 argv[ optind - 1 ] );

				usage_fprint(
				 stdout );

				return( EXIT_FAILURE );

			case (libcstring_system_integer_t) 'h':
				usage_fprint(
				 stdout );

				return( EXIT_SUCCESS );

			case (libcstring_system_integer_t) 'v':
				verbose = 1;

				break;

			case (libcstring_system_integer_t) 'V':
				smrawoutput_copyright_fprint(
				 stdout );

				return( EXIT_SUCCESS );
		}
	}
	if( optind == argc )
	{
		fprintf(
		 stderr,
		 "Missing SMRAW image file(s).\n" );


		usage_fprint(
		 stdout );

		return( EXIT_FAILURE );
	}
	if( ( optind + 1 ) == argc )
	{
		fprintf(
		 stderr,
		 "Missing mount point.\n" );

		usage_fprint(
		 stdout );

		return( EXIT_FAILURE );
	}
	mount_point = argv[ argc - 1 ];

	libsystem_notify_set_verbose(
	 verbose );
	libsmraw_notify_set_stream(
	 stderr,
	 NULL );
	libsmraw_notify_set_verbose(
	 verbose );

#if !defined( LIBSYSTEM_HAVE_GLOB )
	if( libsystem_glob_initialize(
	     &glob,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to initialize glob.\n" );

		goto on_error;
	}
	if( libsystem_glob_resolve(
	     glob,
	     &( argv[ optind ] ),
	     argc - optind - 1,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to resolve glob.\n" );

		goto on_error;
	}
	argv_filenames      = glob->result;
	number_of_filenames = glob->number_of_results;
#else
	argv_filenames      = &( argv[ optind ] );
	number_of_filenames = argc - optind - 1;
#endif

	if( mount_handle_initialize(
	     &smrawmount_mount_handle,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to initialize mount handle.\n" );

		goto on_error;
	}
	if( mount_handle_open_input(
	     smrawmount_mount_handle,
	     argv_filenames,
	     number_of_filenames,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to open SMRAW file(s).\n" );

		goto on_error;
	}
#if defined( HAVE_LIBFUSE )
	if( memory_set(
	     &smrawmount_fuse_operations,
	     0,
	     sizeof( struct fuse_operations ) ) == NULL )
	{
		fprintf(
		 stderr,
		 "Unable to clear fuse operations.\n" );

		goto on_error;
	}
	smrawmount_fuse_operations.open    = &smrawmount_fuse_open;
	smrawmount_fuse_operations.read    = &smrawmount_fuse_read;
	smrawmount_fuse_operations.readdir = &smrawmount_fuse_readdir;
	smrawmount_fuse_operations.getattr = &smrawmount_fuse_getattr;

	smrawmount_fuse_channel = fuse_mount(
	                           mount_point,
	                           NULL );

	if( smrawmount_fuse_channel == NULL )
	{
		fprintf(
		 stderr,
		 "Unable to create fuse channel.\n" );

		goto on_error;
	}
	smrawmount_fuse_handle = fuse_new(
	                          smrawmount_fuse_channel,
	                          NULL,
	                          &smrawmount_fuse_operations,
	                          sizeof( struct fuse_operations ),
	                          smrawmount_mount_handle );
	               
	if( smrawmount_fuse_handle == NULL )
	{
		fprintf(
		 stderr,
		 "Unable to create fuse handle.\n" );

		goto on_error;
	}
	if( verbose == 0 )
	{
		if( fuse_daemonize(
		     0 ) != 0 )
		{
			fprintf(
			 stderr,
			 "Unable to daemonize fuse.\n" );

			goto on_error;
		}
	}
	result = fuse_loop(
	          smrawmount_fuse_handle );

	if( result != 0 )
	{
		fprintf(
		 stderr,
		 "Unable to run fuse loop.\n" );

		goto on_error;
	}
	fuse_destroy(
	 smrawmount_fuse_handle );

	return( EXIT_SUCCESS );
#else
	fprintf(
	 stderr,
	 "No sub system to mount SMRAW format.\n" );

	return( EXIT_FAILURE );
#endif

on_error:
	if( error != NULL )
	{
		libsystem_notify_print_error_backtrace(
		 error );
		liberror_error_free(
		 &error );
	}
#if defined( HAVE_LIBFUSE )
	if( smrawmount_fuse_handle != NULL )
	{
		fuse_destroy(
		 smrawmount_fuse_handle );
	}
#endif
	if( smrawmount_mount_handle != NULL )
	{
		mount_handle_free(
		 &smrawmount_mount_handle,
		 NULL );
	}
	return( EXIT_FAILURE );
}

