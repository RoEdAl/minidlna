/* MiniDLNA media server
 * Copyright (C) 2008-2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>

#include "minidlnatypes.h"
#include "upnpglobalvars.h"
#include "utils.h"
#include "log.h"

int
xasprintf(char **strp, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(strp, fmt, args);
	va_end(args);
	if( ret < 0 )
	{
		DPRINTF(E_WARN, L_GENERAL, "xasprintf: allocation failed\n");
		*strp = NULL;
	}
	return ret;
}

int
ends_with(const char * haystack, const char * needle)
{
	const char * end;
	int nlen = strlen(needle);
	int hlen = strlen(haystack);

	if( nlen > hlen )
		return 0;
 	end = haystack + hlen - nlen;

	return (strcasecmp(end, needle) ? 0 : 1);
}

char *
trim(char *str)
{
	int i;
	int len;

	if (!str)
		return(NULL);

	len = strlen(str);
	for (i=len-1; i >= 0 && isspace(str[i]); i--)
	{
		str[i] = '\0';
		len--;
	}
	while (isspace(*str))
	{
		str++;
		len--;
	}

	if (str[0] == '"' && str[len-1] == '"')
	{
		str[0] = '\0';
		str[len-1] = '\0';
		str++;
	}

	return str;
}

/* Find the first occurrence of p in s, where s is terminated by t */
char *
strstrc(const char *s, const char *p, const char t)
{
	char *endptr;
	size_t slen, plen;

	endptr = strchr(s, t);
	if (!endptr)
		return strstr(s, p);

	plen = strlen(p);
	slen = endptr - s;
	while (slen >= plen)
	{
		if (*s == *p && strncmp(s+1, p+1, plen-1) == 0)
			return (char*)s;
		s++;
		slen--;
	}

	return NULL;
} 

char *
strcasestrc(const char *s, const char *p, const char t)
{
	char *endptr;
	size_t slen, plen;

	endptr = strchr(s, t);
	if (!endptr)
		return strcasestr(s, p);

	plen = strlen(p);
	slen = endptr - s;
	while (slen >= plen)
	{
		if (*s == *p && strncasecmp(s+1, p+1, plen-1) == 0)
			return (char*)s;
		s++;
		slen--;
	}

	return NULL;
} 

/* Copies characters from one buffer to another; size is maximum size of dst buffer */
void
x_strlcpy(char *dst, const char *src, size_t size)
{
	while (--size > 0 && *src != '\0') {
		*dst++ = *src++;
	}
	*dst = '\0';
}

/* Appends characters from one buffer to another; size is maximum size of dst buffer */
void
x_strlcat(char *dst, const char *src, size_t size)
{
	while (size > 0 && *dst != '\0') {
		size--;
		dst++;
	}

	x_strlcpy(dst, src, size);
}

char *
modifyString(char *string, const char *before, const char *after, int noalloc)
{
	int oldlen, newlen, chgcnt = 0;
	char *s, *p;

	/* If there is no match, just return */
	s = strstr(string, before);
	if (!s)
		return string;

	oldlen = strlen(before);
	newlen = strlen(after);
	if (newlen > oldlen)
	{
		if (noalloc)
			return string;

		while ((p = strstr(s, before)))
		{
			chgcnt++;
			s = p + oldlen;
		}
		s = realloc(string, strlen(string)+((newlen-oldlen)*chgcnt)+1);
		/* If we failed to realloc, return the original alloc'd string */
		if( s )
			string = s;
		else
			return string;
	}

	s = string;
	while (s)
	{
		p = strstr(s, before);
		if (!p)
			return string;
		memmove(p + newlen, p + oldlen, strlen(p + oldlen) + 1);
		memcpy(p, after, newlen);
		s = p + newlen;
	}

	return string;
}

char *
unescape_tag(const char *tag, int force_alloc)
{
	char *esc_tag = NULL;

	if (strchr(tag, '&') &&
	    (strstr(tag, "&amp;") || strstr(tag, "&lt;") || strstr(tag, "&gt;") ||
	     strstr(tag, "&quot;") || strstr(tag, "&apos;")))
	{
		esc_tag = strdup(tag);
		esc_tag = modifyString(esc_tag, "&amp;", "&", 1);
		esc_tag = modifyString(esc_tag, "&lt;", "<", 1);
		esc_tag = modifyString(esc_tag, "&gt;", ">", 1);
		esc_tag = modifyString(esc_tag, "&quot;", "\"", 1);
		esc_tag = modifyString(esc_tag, "&apos;", "'", 1);
	}
	else if( force_alloc )
		esc_tag = strdup(tag);

	return esc_tag;
}

char *
escape_tag(const char *tag, int force_alloc)
{
	char *esc_tag = NULL;

	if( strchr(tag, '&') || strchr(tag, '<') || strchr(tag, '>') || strchr(tag, '"') )
	{
		esc_tag = strdup(tag);
		esc_tag = modifyString(esc_tag, "&", "&amp;amp;", 0);
		esc_tag = modifyString(esc_tag, "<", "&amp;lt;", 0);
		esc_tag = modifyString(esc_tag, ">", "&amp;gt;", 0);
		esc_tag = modifyString(esc_tag, "\"", "&amp;quot;", 0);
	}
	else if( force_alloc )
		esc_tag = strdup(tag);

	return esc_tag;
}

char *
strip_ext(char *name)
{
	return strip_char(name, '.');
}

char *
strip_char(char *name, char c)
{
	char *last_occurence = strrchr(name, c);
	if (last_occurence)
		*last_occurence = '\0';

	return last_occurence;
}

/* Code basically stolen from busybox */
int make_dir(const char *path, mode_t mode) {
	char *path_copy = strdup(path);
	char * s = path_copy;
	char c;
	struct stat st;

	do {
		c = '\0';

		/* Before we do anything, skip leading /'s, so we don't bother
		 * trying to create /. */
		while (*s == '/')
			++s;

		/* Bypass leading non-'/'s and then subsequent '/'s. */
		while (*s) {
			if (*s == '/') {
				do {
					++s;
				} while (*s == '/');
				c = *s; /* Save the current char */
				*s = '\0'; /* and replace it with nul. */
				break;
			}
			++s;
		}

		if (mkdir(path_copy, mode) < 0) {
			/* If we failed for any other reason than the directory
			 * already exists, output a diagnostic and return -1.*/
			if ((errno != EEXIST && errno != EISDIR)
					|| (stat(path_copy, &st) < 0 || !S_ISDIR(st.st_mode))) {
				DPRINTF(E_WARN, L_GENERAL, "make_dir: cannot create directory '%s'\n", path_copy);
				free(path_copy);
				return -1;
			}
		}

		if (!c) {
			free(path_copy);
			return 0;
		}

		/* Remove any inserted nul from the path. */
		*s = c;

	} while (1);
}

int make_dir_ex(const char *full_path, mode_t mode) {
	char *full_path_copy = strdup(full_path);
	int res = make_dir(dirname(full_path_copy), mode);
	free(full_path_copy);
	return res;
}

static const size_t _BUF_SIZE = 1024 * 4;

int
copy_file(const char *src_file, const char *dst_file)
{
	size_t nread;
	size_t nwritten = 0;
	size_t size = 0;
	void* buf;

	if (!(buf = malloc(_BUF_SIZE)))
	{
		DPRINTF(E_WARN, L_ARTWORK, "copying %s to %s failed - unable to allocate buffer\n", src_file, dst_file);
		return -1;
	}

	make_dir_ex(dst_file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

	FILE *fsrc = fopen(src_file, "rb");
	FILE *fdst = fopen(dst_file, "wb");

	while ((nread = fread(buf, 1, _BUF_SIZE, fsrc)) > 0)
	{
		size += nread;
		nwritten += fwrite(buf, 1, nread, fdst);
	}

	fclose(fsrc);
	fclose(fdst);

	if (nwritten != size)
	{
		DPRINTF(E_WARN, L_ARTWORK, "copying %s to %s failed [%s]\n", src_file, dst_file, strerror(errno));
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

int
link_file(const char *src_file, const char *dst_file)
{
	if (link(src_file, dst_file) == 0)
		return 0;

	if (errno == ENOENT)
	{
		make_dir_ex(dst_file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		if (link(src_file, dst_file) == 0)
			return 0;
		/* try a softlink if all else fails */
		if (symlink(src_file, dst_file) == 0)
			return 0;
	}
	DPRINTF(E_INFO, L_GENERAL, "Linking %s to %s failed [%s]\n", src_file, dst_file, strerror(errno));
	return -1;
}

/* Simple, efficient hash function from Daniel J. Bernstein */
unsigned int
DJBHash(const uint8_t *data, int len)
{
	unsigned int hash = 5381;
	unsigned int i = 0;

	for(i = 0; i < len; data++, i++)
	{
		hash = ((hash << 5) + hash) + (*data);
	}

	return hash;
}

const char *
mime_to_ext(const char * mime)
{
	switch( *mime )
	{
		/* Audio extensions */
		case 'a':
			if( strcmp(mime+6, "mpeg") == 0 )
				return "mp3";
			else if( strcmp(mime+6, "mp4") == 0 )
				return "m4a";
			else if( strcmp(mime+6, "x-ms-wma") == 0 )
				return "wma";
			else if( strcmp(mime+6, "x-flac") == 0 )
				return "flac";
			else if( strcmp(mime+6, "flac") == 0 )
				return "flac";
			else if( strcmp(mime+6, "x-wav") == 0 )
				return "wav";
			else if( strncmp(mime+6, "L16", 3) == 0 )
				return "pcm";
			else if( strcmp(mime+6, "3gpp") == 0 )
				return "3gp";
			else if( strcmp(mime+6, "ogg") == 0 )
				return "ogg";
			else if( strcmp(mime, "application/ogg") == 0 )
				return "ogg";
#ifdef HAVE_WAVPACK
			else if ( strcmp(mime+6, "x-wavpack") == 0  )
				return "wv";
#endif
			break;
		case 'v':
			if( strcmp(mime+6, "avi") == 0 )
				return "avi";
			else if( strcmp(mime+6, "divx") == 0 )
				return "avi";
			else if( strcmp(mime+6, "x-msvideo") == 0 )
				return "avi";
			else if( strcmp(mime+6, "mpeg") == 0 )
				return "mpg";
			else if( strcmp(mime+6, "mp4") == 0 )
				return "mp4";
			else if( strcmp(mime+6, "x-ms-wmv") == 0 )
				return "wmv";
			else if( strcmp(mime+6, "x-matroska") == 0 )
				return "mkv";
			else if( strcmp(mime+6, "x-mkv") == 0 )
				return "mkv";
			else if( strcmp(mime+6, "x-flv") == 0 )
				return "flv";
			else if( strcmp(mime+6, "vnd.dlna.mpeg-tts") == 0 )
				return "mpg";
			else if( strcmp(mime+6, "quicktime") == 0 )
				return "mov";
			else if( strcmp(mime+6, "3gpp") == 0 )
				return "3gp";
			else if( strncmp(mime+6, "x-tivo-mpeg", 11) == 0 )
				return "TiVo";
			break;
		case 'i':
			if( strcmp(mime+6, "jpeg") == 0 )
				return "jpg";
			else if( strcmp(mime+6, "png") == 0 )
				return "png";
			break;
		default:
			break;
	}
	return "dat";
}

static const mime_info_t MI_MPEG = { "mp3", "audio/mpeg" };
static const mime_info_t MI_MP4 = { "aac", "audio/mp4" };
static const mime_info_t MI_3GPP_AUDIO = { "aac", "audio/3gpp" };
static const mime_info_t MI_MS_WMA = { "asf", "audio/x-ms-wma" };
static const mime_info_t MI_FLAC = { "flc", "audio/flac" };
static const mime_info_t MI_WAV = { "wav", "audio/x-wav" };
static const mime_info_t MI_OGG_AUDIO = { "ogg", "audio/ogg" };
static const mime_info_t MI_L16 = { "pcm", "audio/L16" };
#ifdef HAVE_WAVPACK
static const mime_info_t MI_WAVPACK = { "wv", "audio/x-wavpack" };
#endif

int ext_to_mime(const char *path, mime_info_ptr *mime_info)
{
	const char* dot_pos = strrchr(path, '.');
	if (dot_pos)
	{
		const char *slash_pos = strrchr(path, '/');
		if (slash_pos > dot_pos) dot_pos = NULL;
	}

	if (!dot_pos)
	{ // no file extension
		return 0;
	}

	const char* ext = dot_pos + 1;

	if( !strcasecmp(ext, "mp3") )
	{
		*mime_info = &MI_MPEG;
	}
	else if( !strcasecmp(ext, "m4a") || !strcasecmp(ext, "mp4") ||
	         !strcasecmp(ext, "aac") || !strcasecmp(ext, "m4p") )
	{
		*mime_info = &MI_MP4;
	}
	else if( !strcasecmp(ext, "3gp") )
	{
		*mime_info = &MI_3GPP_AUDIO;
	}
	else if( !strcasecmp(ext, "wma") || !strcasecmp(ext, "asf") )
	{
		*mime_info = &MI_MS_WMA;
	}
	else if( !strcasecmp(ext, "flac") || !strcasecmp(ext, "fla") || !strcasecmp(ext, "flc") )
	{
		*mime_info = &MI_FLAC;
	}
	else if( !strcasecmp(ext, "wav") )
	{
		*mime_info = &MI_WAV;
	}
	else if( !strcasecmp(ext, "ogg") || !strcasecmp(ext, "oga") )
	{
		*mime_info = &MI_OGG_AUDIO;
	}
	else if( !strcasecmp(ext, "pcm") )
	{
		*mime_info = &MI_L16;
	}
#ifdef HAVE_WAVPACK
	else if ( !strcasecmp(ext, "wv") )
	{
		*mime_info = &MI_WAVPACK;
	}
#endif
	else
	{
		return 0;
	}

	return 1;
} 

int
is_video(const char * file)
{
	return (ends_with(file, ".mpg") || ends_with(file, ".mpeg")  ||
		ends_with(file, ".avi") || ends_with(file, ".divx")  ||
		ends_with(file, ".asf") || ends_with(file, ".wmv")   ||
		ends_with(file, ".mp4") || ends_with(file, ".m4v")   ||
		ends_with(file, ".mts") || ends_with(file, ".m2ts")  ||
		ends_with(file, ".m2t") || ends_with(file, ".mkv")   ||
		ends_with(file, ".vob") || ends_with(file, ".ts")    ||
		ends_with(file, ".flv") || ends_with(file, ".xvid")  ||
#ifdef TIVO_SUPPORT
		ends_with(file, ".TiVo") ||
#endif
		ends_with(file, ".mov") || ends_with(file, ".3gp"));
}

int
is_audio(const char * file)
{
	return (ends_with(file, ".mp3") || ends_with(file, ".flac") ||
		ends_with(file, ".wma") || ends_with(file, ".asf")  ||
		ends_with(file, ".fla") || ends_with(file, ".flc")  ||
		ends_with(file, ".m4a") || ends_with(file, ".aac")  ||
		ends_with(file, ".mp4") || ends_with(file, ".m4p")  ||
		ends_with(file, ".wav") || ends_with(file, ".ogg")  ||
#ifdef HAVE_WAVPACK
		ends_with(file, ".wv") ||
#endif
		ends_with(file, ".pcm") || ends_with(file, ".3gp"));
}

int
is_image(const char * file)
{
	return (ends_with(file, ".jpg") || ends_with(file, ".jpeg"));
}

int
is_playlist(const char * file)
{
	return (ends_with(file, ".m3u") || ends_with(file, ".pls"));
}

int
is_caption(const char * file)
{
	return (ends_with(file, ".srt") || ends_with(file, ".smi"));
}

int
is_metadata(const char * file)
{
	return ends_with(file, ".nfo");
}

int
is_album_art(const char * name)
{
	struct linked_names_s * album_art_name;

	/* Check if this file name matches one of the default album art names */
	for( album_art_name = album_art_names; album_art_name; album_art_name = album_art_name->next )
	{
		if( album_art_name->wildcard )
		{
			if( strncmp(album_art_name->name, name, strlen(album_art_name->name)) == 0 )
				break;
		}
		else
		{
			if( strcmp(album_art_name->name, name) == 0 )
				break;
		}
	}

	return (album_art_name ? 1 : 0);
}

enum file_types
resolve_unknown_type(const char * path, media_types dir_type)
{
	struct stat entry;
	unsigned char type = TYPE_UNKNOWN;
	char str_buf[PATH_MAX];
	ssize_t len;

	if( lstat(path, &entry) == 0 )
	{
		if( S_ISLNK(entry.st_mode) )
		{
			if( (len = readlink(path, str_buf, PATH_MAX-1)) > 0 )
			{
				str_buf[len] = '\0';
				//DEBUG DPRINTF(E_DEBUG, L_GENERAL, "Checking for recursive symbolic link: %s (%s)\n", path, str_buf);
				if( strncmp(path, str_buf, strlen(str_buf)) == 0 )
				{
					DPRINTF(E_DEBUG, L_GENERAL, "Ignoring recursive symbolic link: %s (%s)\n", path, str_buf);
					return type;
				}
			}
			stat(path, &entry);
		}

		if( S_ISDIR(entry.st_mode) )
		{
			type = TYPE_DIR;
		}
		else if( S_ISREG(entry.st_mode) )
		{
			if ((dir_type & TYPE_AUDIO) && (is_audio(path) || is_playlist(path))) type = TYPE_FILE;
			if ((dir_type & TYPE_VIDEO) && is_video(path)) type = TYPE_FILE;
			if ((dir_type & TYPE_IMAGES) && is_image(path)) type = TYPE_FILE;
			if ((dir_type & TYPE_RESCAN) && (is_caption(path) || is_metadata(path) || is_image(path))) type = TYPE_FILE;
		}
	}
	return type;
}

enum file_types
resolve_file_type(const struct dirent* dirent, const char *path, media_types dir_types)
{
	if (is_dir(dirent) == 1) return TYPE_DIR;
	if (is_reg(dirent) == 1) return TYPE_FILE;

	return resolve_unknown_type(path, dir_types);
}

