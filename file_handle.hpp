/**
 * @file	file_handle.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/08/28
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_FILE_HANDLE_HPP__
#define __GRIDA_FILE_HANDLE_HPP__

#include <stdint.h>

#if defined(_MSC_VER) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
#include <windows.h>
#endif

#if !defined(_TCHAR_DEFINED)
#define LPCTSTR const char*
#endif

namespace grida {

    class FileHandle {
	private:
		FileHandle(const FileHandle&) = delete;
		FileHandle(FileHandle&&) = delete;

    public:
		class UniqueReadLock {
		private:
			FileHandle& file_handle_;
			bool locked_;
			int lock_rc_;

		public:
			UniqueReadLock(FileHandle& file_handle) : file_handle_(file_handle), locked_(false) {
				lock_rc_ = file_handle_.readLock();
				if (lock_rc_ == 0) {
					locked_ = true;
				}
			}

			~UniqueReadLock() {
				if (locked_) {
					file_handle_.readUnlock();
					locked_ = false;
				}
			}

			int lock_rc() const {
				return lock_rc_;
			}
		};
		class UniqueWriteLock {
		private:
			FileHandle& file_handle_;
			bool locked_;
			int lock_rc_;

		public:
			UniqueWriteLock(FileHandle& file_handle) : file_handle_(file_handle), locked_(false) {
				lock_rc_ = file_handle_.writeLock();
				if (lock_rc_ == 0) {
					locked_ = true;
				}
			}

			~UniqueWriteLock() {
				if (locked_) {
					file_handle_.writeUnlock();
					locked_ = false;
				}
			}

			int lock_rc() const {
				return lock_rc_;
			}
		};

        enum SeekMethod {
			SEEK_FILE_BEGIN,
			SEEK_FILE_CURRENT,
            SEEK_FILE_END
        };

        enum OpenMode {
            MODE_READ = 0x01,
            MODE_WRITE = 0x02,
            MODE_RW = MODE_READ | MODE_WRITE
        };

		FileHandle() = default;
		virtual ~FileHandle() = default;

		/**
		 * Lock read operation
		 * @return
		 */
		virtual int readLock() = 0;

		/**
		 * Unlock read operation
		 * @return
		 */
		virtual int readUnlock() = 0;

		/**
		 * Lock write operation
		 * @return
		 */
		virtual int writeLock() = 0;

		/**
		 * Unlock write operation
		 * @return
		 */
		virtual int writeUnlock() = 0;

		/**
		 * 
		 */
		virtual int setFileSize(int64_t size) = 0;

		/**
		 *
		 */
		virtual int getFileSize(int64_t* psize) = 0;

        /**
         *
         * @param offset
         * @param method
         * @return
         */
        virtual int seek(int64_t offset, SeekMethod method) = 0;

        /**
         * Read data from file
         * @param buf buffer pointer
         * @param length bytes
         * @return read bytes
         */
        virtual int64_t read(void *buf, size_t length) = 0;

        /**
         * Write data to file
         * @param buf buffer pointer
         * @param length bytes
         * @return written bytes
         */
        virtual int64_t write(const void *buf, size_t length) = 0;

        /**
         * Close file
         * @return
         */
        virtual int close() = 0;
    };

} // namespace grida

#endif //__GRIDA_FILE_HANDLE_HPP__
