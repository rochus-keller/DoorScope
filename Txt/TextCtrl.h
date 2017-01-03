#ifndef _TEXT_TEXTCTRL
#define _TEXT_TEXTCTRL

/*
* Copyright 2005-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the DoorScope application
* see <http://doorscope.ch/>).
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <Txt/TextView.h>
#include <QWidget>

namespace Txt
{
	class TextCtrl : public QObject
	{
		Q_OBJECT
	public:
		enum CursorAction {
			DontMove,
			MoveBackward,
			MoveForward,
			MoveWordBackward,
			MoveWordForward,
			MoveUp,
			MoveDown,
			MoveLineStart,
			MoveLineEnd,
			MoveHome,
			MoveEnd,
			MovePageUp,
			MovePageDown,
			MoveNextCell,
			MovePrevCell
		};

		TextCtrl( QObject* owner, TextView* );
		TextView* view() const { return d_view; }
		TextView* getView() const { return d_view; }
		void setPos( qint16 x, qint16 y ); // y kann negativ werden!
		qint16 getDx() const { return d_vr.x(); }
		qint16 getDy() const { return d_vr.y(); }
	signals:
		void scrollOffChanged( quint16 );
		void extentChanged();
		void invalidate( QRect );
		void anchorActivated( QByteArray ); // encoded
	public slots:
		void setScrollOff( quint16 );
	protected slots:
		void handleInvalidate( const QRectF& );
		void handleCursorChanged();
		void handleExtentChanged();
	public: // Hier public, damit auch anderweitig verwendbar
		bool keyPressEvent(QKeyEvent *e);
		bool mousePressEvent(QMouseEvent *e);
		bool mouseMoveEvent(QMouseEvent *e);
		bool mouseReleaseEvent(QMouseEvent *);
		bool mouseDoubleClickEvent(QMouseEvent *e);
		void handleResize(const QSize&);
		void handleFocusIn();
		void handleFocusOut();
		void paint( QPainter *painter, const QPalette& );
	private:
		void ensureVisible(const QRect &rect);
		int visibleHeight();
		bool cursorMoveKeyEvent(QKeyEvent *e);

		TextView* d_view;
		QRect d_vr; // visible Rect. explizit gesetzt.
		bool d_mousePressed;
		quint16 d_yOff; // offset relativ zu top der view
	};
}

#endif // _TEXT_TEXTCTRL
