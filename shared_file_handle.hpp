/**
 * @file	shared_file_handle.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/06
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SHARED_FILE_HANDLE_HPP__
#define __GRIDA_SHARED_FILE_HANDLE_HPP__

#include <libgrida/file_handle.hpp>

#include <memory>

namespace grida {

	class SharedFileHandle : public FileHandle {
	public:
		class RootHandle {
		public:
			virtual std::unique_ptr<FileHandle> subReadModeOpen() = 0;
		};
		virtual std::shared_ptr<RootHandle> rootHandle() = 0;
	};

}

#endif //__ZERON_GRIDA_WIN_FILE_HANDLE_HPP__
