#include <archive_entry.h>

#include "fuse_error.h"
#include "file_backend.h"

static inline void archiveOpen(struct archive *fp, const char *fn) {
    if (archive_read_open_filename(fp, fn, 16777216) != 0) {
        auto err = archive_errno(fp);
        auto msg = std::string(archive_error_string(fp));

        /* free the archive before throwing the error */
        archive_free(fp);
        throw FuseError(err, std::move(msg));
    }
}

FileBackend::~FileBackend() {
    archive_read_close(fp);
    archive_read_free(fp);
}

FileBackend::FileBackend(const std::string &fname) : fp(archive_read_new()) {
    archive_read_support_filter_all(fp);
    archive_read_support_format_all(fp);
    archiveOpen(fp, fname.c_str());
}

void FileBackend::foreach(std::function<void(std::string, struct stat, ByteBuffer)> &&func) const {
    int                    ret;
    struct archive_entry * val;

    /* read all the headers */
    while ((ret = archive_read_next_header(fp, &val)) == ARCHIVE_OK) {
        size_t       len;
        la_int64_t   off;
        ByteBuffer   buf;
        const void * rbuf;

        /* reserve space if possible */
        if (archive_entry_size_is_set(val)) {
            buf.ensure(archive_entry_size(val));
        }

        /* get the file name and stat */
        auto stat = *archive_entry_stat(val);
        auto name = std::string(archive_entry_pathname_utf8(val));

        /* read one file */
        while ((ret = archive_read_data_block(fp, &rbuf, &len, &off)) == ARCHIVE_OK) {
            buf.write(rbuf, len, buf.len());
        }

        /* invoke the callback if needed */
        if (ret == ARCHIVE_EOF) {
            func(std::move(name), stat, std::move(buf));
        } else {
            throw FuseError(archive_errno(fp), archive_error_string(fp));
        }
    }

    /* check for EOF */
    if (ret != ARCHIVE_EOF) {
        throw FuseError(archive_errno(fp), archive_error_string(fp));
    }
}
