/* Copyright (C) 2005 J.F.Dockes 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _FRAGBUTS_H_INCLUDED_
#define _FRAGBUTS_H_INCLUDED_
#include <QWidget>

/* 
 * Display a series of user-defined buttons which activate query
 * language fragments to augment the current search
 */
class FragButs : public QWidget
{
    Q_OBJECT;

 public:

    FragButs(QWidget* parent = 0);
    virtual ~FragButs();

 public slots:
	
 signals:

 private:
    // Detect source file change
    time_t m_reftime; 
 };


#endif /* _FRAGBUTS_H_INCLUDED_ */
