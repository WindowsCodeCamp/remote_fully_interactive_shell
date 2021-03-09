#pragma once
#include <string>
#include <vector>
#include <cassert>
#include <comutil.h>
#pragma comment(lib, "comsuppw.lib")

/*
// 移除字符串两侧的空白字符 '\t', '\n', ' '
static std::string trim(std::string& s);

//字符串截取
//eg.
auto x = sub_string("Hello Apple!", "Hello", "!"); // x=" Apple";
static std::string sub_string(const std::string& src, const std::string& s, const std::string& e);

// 全局替换
static std::string replace_all(std::string str, const std::string& from, const std::string& to);

// 字符串startsWith, endsWith
static bool starts_with(const std::string& str, const std::string& with);
static bool ends_with(const std::string& str, const std::string& with);

// 字符串编码转换
static bool is_utf8(const std::string& str);
static std::wstring mutibyte_to_unicode(const std::string& str, int cp);
static std::string unicode_to_mutibyte(const std::wstring& wstr, int cp);
static std::wstring utf8_to_unicode(const std::string& str);
static std::string unicode_to_utf8(const std::wstring& str);
static std::string ws2s(const std::wstring & s); // 慎用，依赖系统默认编码，多语言环境可能产生乱码
static std::wstring s2ws(const std::string & s); // 慎用，依赖系统默认编码，多语言环境可能产生乱码

// URI编解码
static std::string uri_decode(const std::string& str);
static std::string uri_decode(const std::string& str);

// split截取
static std::vector<S> split(const S& s, const S& delim)
*/

namespace common {
    struct string_utils {
        template<class T>
        static T trim(T& s) {
            T::value_type _t[] = { '\t', '\n', ' ', '\0' };
            s.erase(0, s.find_first_not_of(_t));
            s.erase(s.find_last_not_of(_t) + 1);
            return s;
        }

        template<class T>
        static T sub_string(const T& src, const T& s, const T& e) {
            auto pos1 = src.find(s);
            if (pos1 == T::npos) {
                return T();
            }
            auto pos2 = src.find(e, pos1 + s.length());
            if (pos2 == T::npos) {
                return T();
            }
            return src.substr(pos1 + s.length(), pos2 - pos1 - s.length());
        }
        template<class T, class C, class D>
        static T sub_string(const T& src, C s, D e) {
            return sub_string(src, T(s), T(e));
        }


        template<class T>
        static T replace_all(T str, const T& from, const T& to) {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != T::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
            return str;
        }
        template<class T, class C, class D>
        static T replace_all(T str, C from, D to) {
            return replace_all(str, T(from), T(to));
        }

        template<class T, class C>
        static bool starts_with(const T& src, C w) {
            return src.compare(0, T(w).length(), T(w).c_str()) == 0;
        }

        template<class T, class C>
        static bool ends_with(const T& src, C w) {
            return src.compare(src.length() - T(w).length(), T(w).length(), T(w).c_str()) == 0;
        }

        static bool is_utf8(const std::string& str) {
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, NULL, 0);
            if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
                return false;
            }
            return true;
        }

        static std::wstring utf8_to_unicode(const std::string& str) {
            return mutibyte_to_unicode(str, CP_UTF8);
        }

        static std::string unicode_to_utf8(const std::wstring& str) {
            return unicode_to_mutibyte(str, CP_UTF8);
        }

        // cp == 65001 CP_UTF8
        // cp == 936 gb2312
        static std::wstring mutibyte_to_unicode(const std::string& str, int cp = CP_ACP) {
            int nwLen = MultiByteToWideChar(cp, 0, str.c_str(), -1, NULL, 0);
            wchar_t * pwBuf = new wchar_t[nwLen + 1];
            ZeroMemory(pwBuf, nwLen * 2 + 2);
            MultiByteToWideChar(cp, 0, str.c_str(), -1, pwBuf, nwLen);
            std::wstring s(pwBuf);
            delete[] pwBuf;
            return s;
        }

        // cp == 65001 CP_UTF8 
        // cp == 936 gb2312
        static std::string unicode_to_mutibyte(const std::wstring& wstr, int cp = CP_ACP) {
            int nLen = WideCharToMultiByte(cp, 0, wstr.c_str(), -1, NULL, NULL, NULL, NULL);
            char * pBuf = new char[nLen + 1];
            ZeroMemory(pBuf, nLen + 1);
            WideCharToMultiByte(cp, 0, wstr.c_str(), -1, pBuf, nLen, NULL, NULL);
            std::string s(pBuf);
            delete[] pBuf;
            return s;
        }

        static std::string ws2s(const std::wstring & s) {
            _bstr_t t = s.c_str();
            return std::string(t);
        }

        static std::wstring s2ws(const std::string & s) {
            _bstr_t t = s.c_str();
            return std::wstring(t);
        }

        static std::string uri_encode(const std::string& str) {
            auto _to_hex = [](unsigned char x) {
                return  x > 9 ? x + 55 : x + 48;
            };

            std::string strTemp = "";
            size_t length = str.length();
            for (size_t i = 0; i < length; i++)
            {
                if (isalnum((unsigned char)str[i]) ||
                    (str[i] == '-') ||
                    (str[i] == '_') ||
                    (str[i] == '.') ||
                    (str[i] == '~'))
                    strTemp += str[i];
                else if (str[i] == ' ')
                    strTemp += "+";
                else
                {
                    strTemp += '%';
                    strTemp += _to_hex((unsigned char)str[i] >> 4);
                    strTemp += _to_hex((unsigned char)str[i] % 16);
                }
            }
            return strTemp;
        }

        static std::string uri_decode(const std::string& str) {
            auto _from_hex = [](unsigned char x) {
                unsigned char y;
                if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
                else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
                else if (x >= '0' && x <= '9') y = x - '0';
                else assert(0);
                return y;
            };

            std::string strTemp = "";
            size_t length = str.length();
            for (size_t i = 0; i < length; i++)
            {
                if (str[i] == '+') strTemp += ' ';
                else if (str[i] == '%')
                {
                    assert(i + 2 < length);
                    unsigned char high = _from_hex((unsigned char)str[++i]);
                    unsigned char low = _from_hex((unsigned char)str[++i]);
                    strTemp += high * 16 + low;
                }
                else strTemp += str[i];
            }
            return strTemp;
        }

		// S should be std::string or std::wstring
		template<class S>
		static std::vector<S> split(const S& s, const S& delim)
		{
			std::vector<S> ret;
			auto start = 0U;
			auto end = s.find(delim);
			while (end != S::npos)
			{
				ret.push_back(s.substr(start, end - start));
				start = end + delim.length();
				end = s.find(delim, start);
			}

			ret.push_back(s.substr(start));
			return ret;
		}
		template<class S, class D>
		static std::vector<S> split(S s, D d) {
			return split(s, S(d));
		}
    }; // end string utils
}; // end common namespace
