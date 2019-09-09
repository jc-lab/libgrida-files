//
// Created by jichan on 2019-08-28.
//

#include "win_file_handle.hpp"

namespace zeron_grida {

	WinFileHandle::WinFileHandle() : handle_(NULL) {
	}

	WinFileHandle::~WinFileHandle() {
		close();
	}

	int WinFileHandle::create(LPCTSTR filepath, FileHandle::OpenMode mode) {
		DWORD dwMode = 0;

		if (mode & MODE_READ) {
			dwMode |= GENERIC_READ;
		}
		if (mode & MODE_WRITE) {
			dwMode |= GENERIC_WRITE;
		}

		close();
		handle_ = ::CreateFile(filepath, dwMode, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
		if (!handle_ || (handle_ == INVALID_HANDLE_VALUE)) {
			return -((int)::GetLastError());
		}

		return 1;
	}

	int WinFileHandle::open(LPCTSTR filepath, FileHandle::OpenMode mode) {
		DWORD dwMode = 0;

		if (mode & MODE_READ) {
			dwMode |= GENERIC_READ;
		}
		if (mode & MODE_WRITE) {
			dwMode |= GENERIC_WRITE;
		}

		close();
		handle_ = ::CreateFile(filepath, dwMode, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (!handle_ || (handle_ == INVALID_HANDLE_VALUE)) {
			return -((int)::GetLastError());
		}

		return 1;
	}

	int WinFileHandle::writeLock() {
		mutex_.lock();
		return 0;
	}

	int WinFileHandle::writeUnlock() {
		mutex_.unlock();
		return 0;
	}

	int WinFileHandle::setFileSize(int64_t size) {
		LARGE_INTEGER cur_pos = { 0 };
		LARGE_INTEGER end_pos = { 0 };
		end_pos.QuadPart = size;
		if (!SetFilePointerEx(handle_, end_pos, &cur_pos, FILE_CURRENT)) {
			return ::GetLastError();
		}
		if (!SetEndOfFile(handle_)) {
			return ::GetLastError();
		}
		SetFilePointerEx(handle_, cur_pos, &cur_pos, FILE_CURRENT);
		return 1;
	}
	
	int WinFileHandle::getFileSize(int64_t* psize) {
		LARGE_INTEGER file_size = { 0 };
		if (!GetFileSizeEx(handle_, &file_size)) {
			return ::GetLastError();
		}
		*psize = file_size.QuadPart;
		return 1;
	}

	int WinFileHandle::seek(int64_t offset, SeekMethod method) {
		LARGE_INTEGER cur_pos = { 0 };
		LARGE_INTEGER seek_pos = { 0 };
		int win_method = FILE_CURRENT;
		seek_pos.QuadPart = offset;
		switch (method) {
		case SeekMethod::SEEK_FILE_BEGIN:
			win_method = FILE_BEGIN;
			break;
		case SeekMethod::SEEK_FILE_END:
			win_method = FILE_END;
			break;
		}
		if (!SetFilePointerEx(handle_, seek_pos, &cur_pos, win_method)) {
			return ::GetLastError();
		}
		return 1;
	}

	int64_t WinFileHandle::read(void* buf, size_t length) {
		DWORD dwReadBytes = 0;
		if (!ReadFile(handle_, buf, length, &dwReadBytes, NULL)) {
			return -((int)::GetLastError());
		}
		return dwReadBytes;
	}
	
	int64_t WinFileHandle::write(const void* buf, size_t length) {
		DWORD dwWrittenBytes = 0;
		if (!WriteFile(handle_, buf, length, &dwWrittenBytes, NULL)) {
			return -((int)::GetLastError());
		}
		return dwWrittenBytes;
	}

	int WinFileHandle::close() {
		if (handle_ && (handle_ != INVALID_HANDLE_VALUE)) {
			::CloseHandle(handle_);
			handle_ = NULL;
			return 1;
		}
		return 2;
	}

}
