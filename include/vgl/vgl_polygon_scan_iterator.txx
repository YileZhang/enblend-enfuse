// This is core/vgl/vgl_polygon_scan_iterator.txx
#ifndef vgl_polygon_scan_iterator_txx_
#define vgl_polygon_scan_iterator_txx_
//:
// \file

#include <math.h>
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/if.hpp>

using std::vector;
using std::sort;
using vigra::Point2D;
using namespace boost::lambda;

//#include "vgl_polygon_scan_iterator.h"
//#include <vcl_cstring.h>
//#include <vcl_cmath.h>
//#include <vcl_iostream.h>
//#include <vcl_cstdlib.h>

// It used to be necessary to add 0.5 to the scanline coordinates
// obtained from a vgl_polygon_scan_iterator. Presumably this had
// something to do with pixels and rendering them, but that issue is
// irrelevant to a polygon_scan_iterator.
//
// I think it is clear what a vgl_polygon_scan_iterator should do: tell
// me which points inside my polygon have integer coordinates.
//
// If you cannot live without a polygon_scan_iterator which offsets
// things by 0.5, make a new class called something like
// \code
//   vgl_polygon_scan_iterator_which_adds_one_half
// \endcode
// and change the value of vgl_polygon_scan_iterator_offset back to 0.5
// for that class.
//
// fsm
//

static const double vgl_polygon_scan_iterator_offset = 0.0; // was 0.5f;

// find minimum of a and b
#undef VGLMIN
#define VGLMIN(a,b) (((a)<(b))?(a):(b))

// find maximum of a and b
#undef VGLMAX
#define VGLMAX(a,b) (((a)>(b))?(a):(b))

// file global used by compare functions of qsort
//static vgl_polygon<float>::sheet_t* chs_float;
//static vgl_polygon<double>::sheet_t* chs_double;

//===============================================================
// comparison routines for qsort
//===============================================================
//static int compare_vertind_float(vgl_polygon_scan_iterator<float>::vertind *u,
//                                 vgl_polygon_scan_iterator<float>::vertind *v)
//{
//  return ( chs_float[u->chainnum][u->vertnum].y() <= chs_float[v->chainnum][v->vertnum].y() ) ? -1 : 1;
//}
//
//static int compare_vertind_double(vgl_polygon_scan_iterator<double>::vertind *u,
//                                  vgl_polygon_scan_iterator<double>::vertind *v)
//{
//  return ( chs_double[u->chainnum][u->vertnum].y() <= chs_double[v->chainnum][v->vertnum].y() ) ? -1 : 1;
//}
//
//static int compare_crossedges_float(vgl_polygon_scan_iterator<float>::crossedge *u,
//                                    vgl_polygon_scan_iterator<float>::crossedge *v)
//{
//  return u->x <= v->x ? -1 : 1;
//}
//
//static int compare_crossedges_double(vgl_polygon_scan_iterator<double>::crossedge *u,
//                                     vgl_polygon_scan_iterator<double>::crossedge *v)
//{
//  return u->x <= v->x ? -1 : 1;
//}
//
//typedef int (* qsort_arg_type)(void const*, void const*);
//
//inline static void local_qsort(vgl_polygon_scan_iterator<float>::vertind* yverts, int numverts, vgl_polygon<float>& p)
//{
//  chs_float = &p[0]; // a hack -- but apparently must do it to use qsort
//  vcl_qsort(yverts, numverts, sizeof(yverts[0]), (qsort_arg_type)compare_vertind_float);
//}
//
//inline static void local_qsort(vgl_polygon_scan_iterator<double>::vertind* yverts, int numverts, vgl_polygon<double>& p)
//{
//  chs_double = &p[0]; // a hack -- but apparently must do it to use qsort
//  vcl_qsort(yverts, numverts, sizeof(yverts[0]), (qsort_arg_type)compare_vertind_double);
//}
//
//inline static void local_qsort(vgl_polygon_scan_iterator<float>::crossedge* crossedges, int numcrossedges)
//{
//  vcl_qsort( crossedges, numcrossedges, sizeof crossedges[0], (qsort_arg_type)compare_crossedges_float);
//}
//
//inline static void local_qsort(vgl_polygon_scan_iterator<double>::crossedge* crossedges, int numcrossedges)
//{
//  vcl_qsort( crossedges, numcrossedges, sizeof crossedges[0], (qsort_arg_type)compare_crossedges_double);
//}

//===============================================================
// Destructor
//===============================================================
template <class IMAGE_ITERATOR>
vgl_polygon_scan_iterator<IMAGE_ITERATOR>::~vgl_polygon_scan_iterator()
{
    delete [] crossedges;
    delete [] yverts;
}

//===============================================================
// Constructor - polygon & boundary flag
//===============================================================
template <class IMAGE_ITERATOR>
vgl_polygon_scan_iterator<IMAGE_ITERATOR>::vgl_polygon_scan_iterator(
        const IMAGE_ITERATOR &iterator,
        vgl_polygon const& face,
        bool boundaryp):
    IMAGE_ITERATOR(iterator),
    boundp(boundaryp),
    have_window(false),
    poly_(face),
    vigra_x(0), vigra_y(0)
{
    init();
}

//===============================================================
// Constructor - polygon, boundary flag and viewing area
//===============================================================
template <class IMAGE_ITERATOR>
vgl_polygon_scan_iterator<IMAGE_ITERATOR>::vgl_polygon_scan_iterator(
        const IMAGE_ITERATOR &iterator,
        vgl_polygon const& face,
        bool boundaryp,
        vgl_roi const& window):
    IMAGE_ITERATOR(iterator),
    boundp(boundaryp),
    win(window),
    have_window(true),
    poly_(face),
    vigra_x(0), vigra_y(0)
{
    init();
}

//===============================================================
// Init - data structures necessary for the scan line
//    conversion.  These initializations are common to all 3
//    constructors.
//===============================================================
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::init()
{
    // count total numverts
    numverts = 0;
    for (unsigned int s = 0; s < poly_.size(); ++s)
        numverts += poly_[s].size();

    unsigned int numchains = poly_.size();
    // return if no vertices in face
    if ( numverts == 0 ) {
        // Make a call to next() return false.
        y0 = 0;
        y1 = -1;
        crossedges = NULL;
        yverts = NULL;
        return;
    }

    // create array for storing edges crossing current scan line
    crossedges = new crossedge[ numverts ];

    // create y-sorted array of vertices
    yverts = new vertind[ numverts ];
    int i = 0;
    for (unsigned int j = 0; j < numchains; j++ )
        for (unsigned int h = 0; h < poly_[j].size(); h++ )
        {
            yverts[ i ].chainnum = j;
            yverts[ i ].vertnum = h;
            i++;
        }
    if ( i != numverts )
        cerr << "Error:  i does not equal numverts!\n";

    // sort vertices by y coordinate
    //local_qsort(yverts, numverts, poly_);
    // Comparison expresion:
    //(poly_[vertind1.chainnum])[vertind1.vertnum].y < poly_[vertind2.chainnum][vertind2.vertnum].y
    sort(yverts, yverts+numverts,
            (bind(&Point2D::y, var(poly_)[bind(&vertind::chainnum, _1)][bind(&vertind::vertnum, _1)])
             <
             bind(&Point2D::y, var(poly_)[bind(&vertind::chainnum, _2)][bind(&vertind::vertnum, _2)]))
    );

    double miny, maxy;   // min and max y coordinate of vertices
    miny = get_y( yverts[ 0 ] );
    maxy = get_y( yverts[ numverts - 1 ] );

    // set y0 and y1 to bottommost and topmost scan lines
    if (have_window) {
        if (boundp)
            y0 = (int)VGLMAX((double)win.getUL().y, floor( miny - vgl_polygon_scan_iterator_offset));
        else
            y0 = (int)VGLMAX((double)win.getUL().y, ceil( miny - vgl_polygon_scan_iterator_offset));

        if (boundp)
            y1 = (int)VGLMIN((double)(win.getLR().y-1), ceil( maxy - vgl_polygon_scan_iterator_offset));
        else
            y1 = (int)VGLMIN((double)(win.getLR().y-1), floor( maxy - vgl_polygon_scan_iterator_offset));
    }
    else {
        if (boundp)
            y0 = (int)floor( miny - vgl_polygon_scan_iterator_offset);
        else
            y0 = (int)ceil( miny - vgl_polygon_scan_iterator_offset);

        if (boundp)
            y1 = (int)ceil( maxy - vgl_polygon_scan_iterator_offset);
        else
            y1 = (int)floor(  maxy - vgl_polygon_scan_iterator_offset);
    }

    reset();
    next();
    vigra_y = scany();
    vigra_x = startx();
    Diff2D delta(vigra_x, vigra_y);
    IMAGE_ITERATOR::operator+=(delta);
}

//===============================================================
// Deletes edge (v,get_next_vert(v)) from the crossedge array
//===============================================================
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::delete_edge( vertind v )
{
    int j;
    for ( j = 0; ( j < numcrossedges ) &&
                 ( !( ( crossedges[j].v.chainnum == v.chainnum ) &&
                      ( crossedges[j].v.vertnum == v.vertnum ) )); j++ );

    // edge not in cross edge list; happens at win->y0
    if ( j >= numcrossedges ) return;

    numcrossedges--;
    memcpy( &crossedges[j], &crossedges[j+1],
            (numcrossedges-j)*sizeof( crossedges[0] ));
}

//===============================================================
// Inserts edge (v,get_next_vert(v)) into the crossedge array
//===============================================================
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::insert_edge( vertind v )
{
    vertind nextvert;
    double dx;
    Point2D p, q;

    get_next_vert( v, nextvert );
    if ( get_y( v ) < get_y( nextvert ) )
    {
        p = get_pt( v );
        q = get_pt( nextvert );
    }
    else
    {
        p = get_pt( nextvert );
        q = get_pt( v );
    }

    // initialize x position at intersection of edge with scanline y
    crossedges[numcrossedges].dx = dx = ((double)q.x - (double)p.x) / ((double)q.y - (double)p.y);
    crossedges[numcrossedges].x = dx * ( fy + vgl_polygon_scan_iterator_offset - (double)p.y ) + (double)p.x;
    crossedges[numcrossedges].v = v;
    numcrossedges++;
}

//===============================================================
// Resets the iterator so that when next() is called, it will
//    store the first scan segment
//===============================================================
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::reset()
{
    y = y0;               // first interior scan line
    numcrossedges = 0;    // start with empty crossingedges list
    curcrossedge = 0;     // crossedge marking first scan segment
    k = 0;                // yverts[k] is next vertex to process
    xl = 0;               // Arbitrary values
    xr = 0;
    fxl = 0;
    fxr = 0;
}

////===============================================================
//// Round the double to the nearest int
////===============================================================
//template <class IMAGE_ITERATOR>
//static inline int irnd(double x)
//{
//  return (int)floor(x + 0.5);
//}

//===============================================================
// Moves iterator to the next scan segment.
// Scanline y is at y+vgl_polygon_scan_iterator_offset.
//
//??? Check vertices between previous scanline and current one, if any to simplify.
//??? If pt.y=y+0.5,  pretend it's above invariant: y-0.5 < pt[i].y <= y+.5.
//===============================================================
template <class IMAGE_ITERATOR>
bool vgl_polygon_scan_iterator<IMAGE_ITERATOR>::next( )
{
    // Find next segment on current scan line
    if ( curcrossedge < numcrossedges )
    {
        fxl = crossedges[curcrossedge].x;
        fxr = crossedges[curcrossedge+1].x;
        if (boundp)
            // left end of span with boundary
            xl = (int)floor( crossedges[curcrossedge].x - vgl_polygon_scan_iterator_offset);
        else
            // left end of span without boundary
            xl = (int)ceil( crossedges[curcrossedge].x - vgl_polygon_scan_iterator_offset);

        //if ( have_window && xl < irnd(win.getUL().x) )
        if ( have_window && xl < win.getUL().x )
        {
            //fxl = win.getUL().x;
            //xl = irnd(fxl);
            xl = win.getUL().x;
            fxl = (double)xl;
        }

        if ( boundp )
            //right end of span with boundary
            xr = (int)ceil( crossedges[curcrossedge+1].x - vgl_polygon_scan_iterator_offset);
        else
            // right end of span without boundary
            xr = (int)floor( crossedges[curcrossedge+1].x - vgl_polygon_scan_iterator_offset);

        //if ( have_window && xr >= irnd(win.getLR().x) )
        if ( have_window && xr >= win.getLR().x )
        {
            //fxr = win.getLR().x -1;
            //xr =  irnd(fxr);
            xr = win.getLR().x -1;
            fxr =  (double)xr;
        }

        // adjust the x coord so that it is the intersection of
        // the edge with the scan line one above current
        crossedges[curcrossedge].x += crossedges[curcrossedge].dx;
        crossedges[curcrossedge+1].x += crossedges[curcrossedge+1].dx;
        curcrossedge+=2;
        if (! ( xl <= xr ) ) {
            cout << "on line " << (y-1) << " xl=" << xl << " > xr=" << xr << endl;
            cout << "on line " << (y-1) << " fxl=" << fxl << " fxr=" << fxr << endl;
            cout << "numcrossedges=" << numcrossedges << " curcrossedge=" << curcrossedge << endl;
            return next();
        } else {
            return true;
        }
    }

    // All segments on current scan line have been exhausted.  Start
    // processing next scan line.
    vertind curvert, prevvert, nextvert;
    if ( y <= y1 )
    {
        // Current scan line is not the last one.
        bool not_last = true;

        // If boundary included and processing first or last scan line
        // floating point scan line must be taken as a min/max y coordinate
        // of the polygon. Otherwise these scan lines are not included because
        // of the earlier rounding (ceil/floor).
        if ( boundp ) {
            if ( y == y0 )
                fy = floor(get_y( yverts[ 0 ] ));
            else if ( y == y1 ) {
                fy = ceil(get_y( yverts[ numverts - 1 ] ));
                not_last = false;
            }
            else
                fy = (double)y;
        }
        else
            fy = (double)y;

        for (; (k<numverts) && (get_y(yverts[k]) <= (fy+vgl_polygon_scan_iterator_offset)) && not_last; k++)
        {
            curvert = yverts[ k ];

            // insert or delete edges (curvert, nextvert) and (prevvert, curvert)
            // from crossedges list if they cross scanline y
            get_prev_vert( curvert, prevvert );

            if ( get_y( prevvert ) <= (fy-vgl_polygon_scan_iterator_offset))  // old edge, remove from active list
                delete_edge( prevvert );
            else if ( get_y( prevvert ) > (fy+vgl_polygon_scan_iterator_offset))  // new edge, add to active list
                insert_edge( prevvert );

            get_next_vert( curvert, nextvert );

            if ( get_y( nextvert ) <= (fy-vgl_polygon_scan_iterator_offset))  // old edge, remove from active list
                delete_edge( curvert );
            else if ( get_y( nextvert ) > (fy+vgl_polygon_scan_iterator_offset))  // new edge, add to active list
                insert_edge( curvert );
        }

        // sort edges crossing scan line by their intersection with scan line
        //local_qsort(crossedges, numcrossedges);
        // u->x <= v->x ? -1 : 1
        sort(crossedges, crossedges+numcrossedges,
                (bind(&crossedge::x, _1) <= bind(&crossedge::x, _2))
                //if_then_else_return(bind(&crossedge::x, _1) <= bind(&crossedge::x, _2),
                //        constant(-1),
                //        constant(1))
        );
        //display_crossedges();

        curcrossedge = 0; // Process the next set of horizontal spans
        y++;
        return next();
    }
    else
        return false;
}

//===============================================================
//: Returns the vertex following v in v's chain.
//  The vertex is returned through the parameter nextvert.
//  I get a syntax error when I tried to return an object of type vertind.
//  Compiler error says the default return type is int???
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::get_next_vert( vertind v, vertind & nextvert )
{
    nextvert = v;
    nextvert.vertnum += 1;
    if ( nextvert.vertnum == poly_[nextvert.chainnum].size() )
        nextvert.vertnum = 0; // wrap around to first vertex
}

//: Returns the vertex preceding v in v's chain.
//  The vertex is returned through the parameter prevvert.
//  I get a syntax error when I tried to return an object of type vertind.
//  Compiler error says the default return type is int???
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::get_prev_vert( vertind v, vertind & prevvert )
{
    prevvert = v;
    if ( prevvert.vertnum == 0 )   // wrap around to last vertex
        prevvert.vertnum = poly_[prevvert.chainnum].size() - 1;
    else
        prevvert.vertnum = prevvert.vertnum - 1;
}

//===============================================================
// For debugging purposes.
//===============================================================
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::display_chains()
{
    cout << "Number of Chains: " << poly_.size() << endl
         << "Number of Vertices: " << numverts << endl;
    for (unsigned int c = 0; c < poly_.size(); ++c )
    {
        cout << "---- Chain # " << c << " ----\n"
             << "  Length: " << poly_[ c ].size() << endl;
        for (unsigned int v = 0; v < poly_[ c ].size(); v++ )
        {
            cout << "  [ " << poly_[ c ][ v ].x
                 << ' ' << poly_[ c ][ v ].y << " ]\n";
        }
    }
    //vcl_cout << vcl_flush;
}

//===============================================================
// For debugging purposes.
//===============================================================
template <class IMAGE_ITERATOR>
void vgl_polygon_scan_iterator<IMAGE_ITERATOR>::display_crossedges()
{
    cout << "----- CROSSEDGES -----\n"
         << "numcrossedges: " << numcrossedges << endl;
    for (int i = 0; i< numcrossedges; i++ )
    {
        cout << "x = " << crossedges[i].x << '\n'
             << "dx = " << crossedges[i].dx << '\n'
             << "v: chainnum=" << crossedges[i].v.chainnum
             << ", vertnum=" << crossedges[i].v.vertnum << '\n';
    }
    cout << "---------------------\n";// << vcl_flush;
}

/*
#undef VGL_POLYGON_SCAN_ITERATOR_INSTANTIATE
#define VGL_POLYGON_SCAN_ITERATOR_INSTANTIATE(T) \
template class vgl_polygon_scan_iterator<T >
*/

#endif // vgl_polygon_scan_iterator_txx_