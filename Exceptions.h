#ifndef __Tk_Exceptions__
#define __Tk_Exceptions__

#include <exception>

namespace Tk
{
	class TkException : public std::exception
	{
	public:
		enum Code {
			InboxDir,
		};
		TkException( Code c, const QString& msg = "" ):d_err(c),d_msg(msg){}
		virtual ~TkException() throw() {}
		Code getCode() const { return d_err; }
		const QString& getMsg() const { return d_msg; }
	private:
		Code d_err;
		QString d_msg;
	};

}

#endif
