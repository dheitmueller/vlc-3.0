/*****************************************************************************
 * ctrl_list.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teuli�re <ipkiss@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <math.h>
#include "ctrl_list.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/generic_font.hpp"
#include "../utils/position.hpp"
#include "../utils/ustring.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_scroll.hpp"
#include "vlc_keys.h"
#ifdef sun
#   include "solaris_specific.h" // for lrint
#endif

#define SCROLL_STEP 0.05
#define LINE_INTERVAL 1  // Number of pixels inserted between 2 lines


CtrlList::CtrlList( intf_thread_t *pIntf, VarList &rList, GenericFont &rFont,
                    uint32_t fgColor, uint32_t playColor, uint32_t bgColor1,
                    uint32_t bgColor2, uint32_t selColor,
                    const UString &rHelp, VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_rList( rList ), m_rFont( rFont ),
    m_fgColor( fgColor ), m_playColor( playColor ), m_bgColor1( bgColor1 ),
    m_bgColor2( bgColor2 ), m_selColor( selColor ), m_pLastSelected( NULL ),
    m_pImage( NULL ), m_lastPos( 0 )
{
    // Observe the list and position variables
    m_rList.addObserver( this );
    m_rList.getPositionVar().addObserver( this );

    makeImage();
}


CtrlList::~CtrlList()
{
    m_rList.getPositionVar().delObserver( this );
    m_rList.delObserver( this );
    if( m_pImage )
    {
        delete m_pImage;
    }
}


void CtrlList::onUpdate( Subject<VarList> &rList )
{
    makeImage();
    notifyLayout();
    m_pLastSelected = NULL;
}


void CtrlList::onUpdate( Subject<VarPercent> &rPercent )
{
    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return;
    }
    int height = pPos->getHeight();

    // How many lines can be displayed ?
    int itemHeight = m_rFont.getSize() + LINE_INTERVAL;
    int maxItems = height / itemHeight;

    // Determine what is the first item to display
    VarPercent &rVarPos = m_rList.getPositionVar();
    int firstItem = 0;
    int excessItems = m_rList.size() - maxItems;
    if( excessItems > 0 )
    {
        // a simple (int)(...) causes rounding errors !
#ifdef _MSC_VER
#   define lrint (int)
#endif
        firstItem = lrint( (1.0 - rVarPos.get()) * (double)excessItems );
    }
    if( m_lastPos != firstItem )
    {
        // Redraw the control if the position has changed
        m_lastPos = firstItem;
        makeImage();
        notifyLayout();
    }
}


void CtrlList::onResize()
{
    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return;
    }
    int height = pPos->getHeight();

    // How many lines can be displayed ?
    int itemHeight = m_rFont.getSize() + LINE_INTERVAL;
    int maxItems = height / itemHeight;

    // Update the position variable
    VarPercent &rVarPos = m_rList.getPositionVar();
    int excessItems = m_rList.size() - maxItems;
    if( excessItems > 0 )
    {
        double newVal = 1.0 - (double)m_lastPos / excessItems;
        if( newVal >= 0 )
        {
            // Change the position to keep the same first displayed item
            rVarPos.set( 1.0 - (double)m_lastPos / excessItems );
        }
        else
        {
            // We cannot keep the current first item
            m_lastPos = excessItems;
        }
    }

    makeImage();
    notifyLayout();
}


void CtrlList::onPositionChange()
{
    makeImage();
    notifyLayout();
}


void CtrlList::handleEvent( EvtGeneric &rEvent )
{
    if( rEvent.getAsString().find( "key:down" ) != string::npos )
    {
        char key = ((EvtKey&)rEvent).getKey();
    }

    else if( rEvent.getAsString().find( "mouse:left" ) != string::npos )
    {
        EvtMouse &rEvtMouse = (EvtMouse&)rEvent;
        const Position *pos = getPosition();
        int yPos = m_lastPos + ( rEvtMouse.getYPos() - pos->getTop() ) /
            (m_rFont.getSize() + LINE_INTERVAL);
        VarList::Iterator it;
        int index = 0;

        if( rEvent.getAsString().find( "mouse:left:down:ctrl,shift" ) !=
                 string::npos )
        {
            // Flag to know if the current item must be selected
            bool select = false;
            for( it = m_rList.begin(); it != m_rList.end(); it++ )
            {
                bool nextSelect = select;
                if( index == yPos || &*it == m_pLastSelected )
                {
                    if( select )
                    {
                        nextSelect = false;
                    }
                    else
                    {
                        select = true;
                        nextSelect = true;
                    }
                }
                (*it).m_selected = (*it).m_selected || select;
                select = nextSelect;
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:down:ctrl" ) !=
                 string::npos )
        {
            for( it = m_rList.begin(); it != m_rList.end(); it++ )
            {
                if( index == yPos )
                {
                    (*it).m_selected = ! (*it).m_selected;
                    m_pLastSelected = &*it;
                    break;
                }
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:down:shift" ) !=
                 string::npos )
        {
            // Flag to know if the current item must be selected
            bool select = false;
            for( it = m_rList.begin(); it != m_rList.end(); it++ )
            {
                bool nextSelect = select;
                if( index == yPos ||  &*it == m_pLastSelected )
                {
                    if( select )
                    {
                        nextSelect = false;
                    }
                    else
                    {
                        select = true;
                        nextSelect = true;
                    }
                }
                (*it).m_selected = select;
                select = nextSelect;
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:down" ) !=
                 string::npos )
        {
            for( it = m_rList.begin(); it != m_rList.end(); it++ )
            {
                if( index == yPos )
                {
                    (*it).m_selected = true;
                    m_pLastSelected = &*it;
                }
                else
                {
                    (*it).m_selected = false;
                }
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:dblclick" ) !=
                 string::npos )
        {
            for( it = m_rList.begin(); it != m_rList.end(); it++ )
            {
                if( index == yPos )
                {
                    (*it).m_selected = true;
                    m_pLastSelected = &*it;
                    // Execute the action associated to this item
                    m_rList.action( &*it );
                }
                else
                {
                    (*it).m_selected = false;
                }
                index++;
            }
        }

        // Redraw the control
        makeImage();
        notifyLayout();
    }

    else if( rEvent.getAsString().find( "scroll" ) != string::npos )
    {
        int direction = ((EvtScroll&)rEvent).getDirection();

        double percentage = m_rList.getPositionVar().get();
        if( direction == EvtScroll::kUp )
        {
            percentage += SCROLL_STEP;
        }
        else
        {
            percentage -= SCROLL_STEP;
        }
        m_rList.getPositionVar().set( percentage );
    }
}


bool CtrlList::mouseOver( int x, int y ) const
{
    const Position *pPos = getPosition();
    if( pPos )
    {
        int width = pPos->getWidth();
        int height = pPos->getHeight();
        return ( x >= 0 && x <= width && y >= 0 && y <= height );
    }
    return false;
}


void CtrlList::draw( OSGraphics &rImage, int xDest, int yDest )
{
    if( m_pImage )
    {
        rImage.drawGraphics( *m_pImage, 0, 0, xDest, yDest );
    }
}


void CtrlList::makeImage()
{
    if( m_pImage )
    {
        delete m_pImage;
    }

    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return;
    }
    int width = pPos->getWidth();
    int height = pPos->getHeight();
    int itemHeight = m_rFont.getSize() + LINE_INTERVAL;

    // Create an image
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    m_pImage = pOsFactory->createOSGraphics( width, height );

    // Current background color
    uint32_t bgColor = m_bgColor1;

    // Draw the background
    VarList::ConstIterator it = m_rList[m_lastPos];
    for( int yPos = 0; yPos < height; yPos += itemHeight )
    {
        int rectHeight = __MIN( itemHeight, height - yPos );
        if( it != m_rList.end() )
        {
            uint32_t color = ( (*it).m_selected ? m_selColor : bgColor );
            m_pImage->fillRect( 0, yPos, width, rectHeight, color );
            it++;
        }
        else
        {
            m_pImage->fillRect( 0, yPos, width, rectHeight, bgColor );
        }
        // Flip the background color
        bgColor = ( bgColor == m_bgColor1 ? m_bgColor2 : m_bgColor1 );
    }

    // Draw the items
    int yPos = 0;
    for( it = m_rList[m_lastPos]; it != m_rList.end() && yPos < height; it++ )
    {
        UString *pStr = (UString*)((*it).m_cString.get());
        uint32_t color = ( (*it).m_playing ? m_playColor : m_fgColor );

        // Draw the text
        GenericBitmap *pText = m_rFont.drawString( *pStr, color, width );
        if( !pText )
        {
            return;
        }
        yPos += itemHeight - pText->getHeight();
        int ySrc = 0;
        if( yPos < 0 )
        {
            ySrc = - yPos;
            yPos = 0;
        }
        int lineHeight = __MIN( pText->getHeight() - ySrc, height - yPos );
        m_pImage->drawBitmap( *pText, 0, ySrc, 0, yPos, pText->getWidth(),
                              lineHeight );
        yPos += (pText->getHeight() - ySrc );
        delete pText;

    }
}

