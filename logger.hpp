/**
 * @file	logger.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_LOGGER_HPP__
#define __GRIDA_LOGGER_HPP__

#include <string>

#include <stdio.h>
#include <stdarg.h>

namespace grida {

    class Logger {
    public:
        virtual void puts(const std::string& text);
        virtual void printf(const char *format, ...);
    };

} // namespace grida

#endif // __GRIDA_DOWNLOAD_CONTEXT_HPP__
