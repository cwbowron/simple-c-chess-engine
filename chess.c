/*
+----
| chess.c
|
| christopher w bowron
|
| bowronch@cse.msu.edu
|
| copyright 1999 by christopher bowron
|
|      -= digital dreamland 1999 =-
| ...we came, we played chess, we left...
|
+----
| - todo -
| get a name
| heuristics to get better moves to top of move list
|      save the better moves from alpha beta to a list
|      to inspect the next time we need a move
|
| greater depth
| book openings
|
| - DONE -
| fix memory leaks - no wasted memory!
| fix pawn promotion - kludged
| manage own memory - no dynamic malloc/free
| print best search result found
| variables as single int using masking
| fix enpassant
| fix random loss of pieces
| speed up incheckp
+----
*/

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>

#include "chess.h"

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
#define PIECECHAR(p) (" PNBRQKpnbrqk"[p])

#define NILMOVE   (makemove(makesquare(8,8),makesquare(8,8)))
#define DRAWREP   (makemove(makesquare(9,9),makesquare(9,9)))   /* (-1) */
#define NPIECES   (13)
#define BLACK     (-1)
#define WHITE     (1)
#define CHECKMATE (3000)
#define STALEMATE (1000)
#define MAXDEPTH  (6)

#define loopboard {int x, y; for(x=0;x<8;x++) for(y=0;y<8;y++)
#define endloop   }

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
#define getx(s)  ( (s) & 0x0f )
#define gety(s)  ( (s) >> 4 )
#define gets1(m) ( (m) & 0xff )
#define gets2(m) ( ((m) >> 8) & 0xff)

#define setpiece(m, p)  ( (*m) += ( (p) << 16 ) )
#define setcheck(m)     ( (*m) |= ( 1 << 31 ) )
#define setmate(m)      ( (*m) |= ( 3 << 30 ) )
#define getpiece(m)     ( (m) >> 16 )
#define makemove(s1,s2) ( ((s2) << 8) | (s1) )
#define makesquare(x,y) ( ((y) << 4) | (x))

#define moveequal(m1, m2)   ( (m1) == (m2) )
#define squareequal(s1, s2) ( (s1) == (s2) )

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
#define whiteoo  makemove (makesquare (4, 0), makesquare (6, 0))
#define whiteooo makemove (makesquare (4, 0), makesquare (2, 0))
#define blackoo  makemove (makesquare (4, 7), makesquare (6, 7))
#define blackooo makemove (makesquare (4, 7), makesquare (2, 7))

#define wthruoo  makemove (makesquare(4,0),makesquare(5,0))
#define bthruoo  makemove (makesquare(4,7),makesquare(5,7))
#define wthruooo makemove (makesquare(4,0),makesquare(3,0))
#define bthruooo makemove (makesquare(4,7),makesquare(3,7))

#define A1 makesquare(0,0)
#define G1 makesquare(7,0)
#define A8 makesquare(0,7)
#define G8 makesquare(7,7)

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
chessboard board =
{
    { wr, wn, wb, wq, wk, wb, wn, wr },
    { wp, wp, wp, wp, wp, wp, wp, wp },
    { empty, empty, empty, empty, empty, empty, empty, empty },
    { empty, empty, empty, empty, empty, empty, empty, empty },
    { empty, empty, empty, empty, empty, empty, empty, empty },
    { empty, empty, empty, empty, empty, empty, empty, empty },
    { bp, bp, bp, bp, bp, bp, bp, bp },
    { br, bn, bb, bq, bk, bb, bn, br }
};

piece bigboard[12][12] =
{
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
    { empty,empty,wr, wn, wb, wq, wk, wb, wn, wr,empty,empty},
    { empty,empty, wp, wp, wp, wp, wp, wp, wp, wp,empty,empty },
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
    { empty,empty, bp, bp, bp, bp, bp, bp, bp, bp,empty,empty },
    { empty,empty, br, bn, bb, bq, bk, bb, bn, br,empty,empty },
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
    { empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty,empty},
};

#define getxy(x,y)      ( bigboard[(y)+2][(x)+2] )
#define setxy(x,y,p)    ( bigboard[(y)+2][(x)+2] = (p) )
#define offboardp(x,y)  ( ((x)&0xfff8) | ((y)&0xfff8) )

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
unsigned int movecount=0;

movelist *moves[2]={NULL,NULL};

int movedepth[2];

int knightdx[]={2, 2,-2,-2};
int knightdy[]={1,-1,-1, 1};

move bestarray[MAXDEPTH];

struct variables globals = { 1, 1, 1, 1, 8, 8, 1 };

int computercontrolled [] = { 0, 0 };

square bking = makesquare (4,7);
square wking = makesquare (4,0);

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* dont use dynamic memory for movelists */
#define STACKSIZE (unsigned)(1<<15)

movelist movestack[STACKSIZE];
char used[STACKSIZE];
int stackpointer=0;

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
#define newlist() (NULL)

movelist *newmovelist()
{
    /*return malloc(sizeof(movelist));*/
    int t = stackpointer;
    int t2 = t;
    
    while (used[t])
    {
	t++;
	t %= STACKSIZE;
	if (t2 == t)
	{
	    printf("out of memory\n");
	    exit(255);
	}
    }
    stackpointer = (t+1) % STACKSIZE;
    used[t]=1;
    return &movestack[t];
}

int meminuse()
{
    int count, x;
    count=0;
    for(x=0;x<STACKSIZE;x++)
    {
	if (used[x])
	    count++;
    }
    return count;
}

void freemovelist(movelist *m)
{
    /*free(m);*/
    int t = (int) m - (int) movestack;
    t /= sizeof(movelist);
    used[t] = 0;
}


void push (movelist **list, move m)
{
    movelist *new = newmovelist();
    new->data = m;
    new->next = *list;
    *list = new;
    return;
}

move pop (movelist **list)
{
    movelist *temp=*list;
    move m;
    
    if (temp == NULL)
	return NILMOVE;
    
    m = temp->data;
    *list = temp->next;
    freemovelist(temp);
    return m;
}

movelist *reverse(movelist *list)
{
    movelist *res=newlist();
    move m;
    
    while(list)
    {
	m = pop(&list);
	push(&res, m);
    }
    return res;
}

int length(movelist *list)
{
    int c=0;
    while(list)
    {
	c++;
	list=list->next;
    }
    return c;
}

void deletelist(movelist *list)
{
    movelist *t;

    while (list)
    {
	t = list;
	list=list->next;
	freemovelist(t);
    }
}
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
void printsquare (square s)
{
    printf("(%d, %d) ", getx(s), gety(s));
}

void printmove (move m)
{
    printsquare (gets1(m));
    printsquare (gets2(m));
    if (getpiece(m) <= NPIECES)
	printf("%c", PIECECHAR( getpiece(m)) );

    if ((m&(3<<30))==(3<<30))
	printf(" ++");
    else if (m&(1<<31))
	printf(" +");

    
    printf("\n");
}
/* print list in coordinate notation */
void printlist (movelist *list)
{
    move m;

    if (list == NULL)
	return;

    m = list->data;

    printmove(m);
    printlist (list->next);
}
/* print list in algebraic notation */
void printalist (movelist *list)
{
    move m;
    char *s;
    int c=0;
    
    while (list)
    {
	c++;
	m = list->data;
	s = algebraic(m);
	printf("%s", s);
	if (c%10 == 0) printf("\n");
	free(s);
	list = list->next;
    }
    if (c%10 != 0) printf("\n");
}

void printboardw()
{
    int x,y;
    printf("    a   b   c   d   e   f   g   h  \n");
    printf("  +---+---+---+---+---+---+---+---+\n");
    for (y=7;y>=0;y--)
    {
	printf("%d |", y+1);
	for (x=0;x<=7;x++)
	{
	    printf(" %c |", PIECECHAR( getxy (x, y) ) );
	}
	printf(" %d \n", y+1);
	printf("  +---+---+---+---+---+---+---+---+\n");
    }
    printf("    a   b   c   d   e   f   g   h  \n");
    fflush(stdout);
}

void printboardb()
{
    int x,y;
    printf("    h   g   f   e   d   c   b   a  \n");
    printf("  +---+---+---+---+---+---+---+---+\n");
    for (y=0;y<=7;y++)
    {
	printf("%d |", y+1);
	for (x=7;x>=0;x--)
	{
	    printf(" %c |", PIECECHAR( getxy (x, y) ) );
	}
	printf(" %d \n", y+1);
	printf("  +---+---+---+---+---+---+---+---+\n");
    }

    printf("    h   g   f   e   d   c   b   a  \n");
    fflush(stdout);
}

void printboard(int color)
{
    if (color == WHITE)
	printboardw();
    else
	printboardb();
}

void printsearchresults(int res, int color)
{
    int x;
    char *s;
    int max = (color == WHITE) ? movedepth[0] : movedepth[1];

    printf("\nSearch Outcome : Adv %d = ", res);
    for (x=max; x>0 ;x--)
    {
	s = algebraic(bestarray[x]);
	printf("%s", s);
	free(s);
    }
    printf("\n");
    return;
}
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* stuff to do before we can play our game */
void initialize ()
{
    /* our movelist stack setup */
    printf("Allocating %d MoveLists\n", sizeof(used) / sizeof(*used));
    memset(used, 0, sizeof(used));
    /* random seed */
    srandom(time(0));
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int getcolor (piece p)
{
    if (p==0) return 0;
    if (p<=wk) return WHITE;
    return BLACK;
}
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int clearpathp(square s1, square s2)
{
    int x1, x2, y1, y2, dx, dy, x, y;
    x1 = getx(s1);
    x2 = getx(s2);
    y1 = gety(s1);
    y2 = gety(s2);

    dx = x2-x1;
    dy = y2-y1;

    if (dy != 0)
    {
	if (dy > 0) dy = 1;
	else dy = -1;
    }
    
    if (dx !=0)
    {
        if (dx > 0) dx = 1;
	else dx = -1;
    }
    
    for (x=x1+dx, y=y1+dy; ; x+=dx, y+=dy)
    {
	if ( ( x == x2 ) && ( y == y2) )
	    return 1;

	if ( offboardp (x, y) )
	    return 1;
	    
	if ( getxy(x, y) != empty )
	    return 0;
    }
    return 1;			/* shouldnt get here though */
}

int pcheck(move m)
{
    int color, direction, start;
    int hor, ver;
    int x1, x2, y1, y2;

    x1 = getx(gets1(m));
    x2 = getx(gets2(m));
    y1 = gety(gets1(m));
    y2 = gety(gets2(m));
    
    hor = x2 - x1;
    ver = y2 - y1;
        
    direction = color =	getcolor(getxy( x1, y1) );
    
    start = (color == WHITE) ? 1 : 6;

    /* can move forward one square */
    if ( (getxy ( x2, y2) == empty) &&
	 (hor == 0) &&
	 (direction == ver) )
	return 1;

    /* move forward 2 squares on first move */
    if ( (getxy ( x2, y2) == empty) &&
	 (getxy ( x2, y2 - direction) == empty) &&
	 (hor == 0) &&
	 (direction * 2 == ver) &&
	 ( y1 == start))
	return 1;
	
    /* attack diagonally */
    if ( (direction == ver) &&
	 (( hor == 1) || (hor == -1)) &&
	 (getcolor (getxy (x2, y2)) == -color))
	return 1;

    /* white enpassant */
    if ( (direction == ver) &&
	 (( hor == 1) || (hor == -1)) &&
	 ( y2 == 5 ) &&
	 ( color == WHITE ) &&
	 ( x2 == globals.wenpassant ) )
	return 1;

    /* black enpassant */
    if ( (direction == ver) &&
	 (( hor == 1) || (hor == -1)) &&
	 ( y2 == 2 ) &&
	 ( color == BLACK ) &&
	 ( x2 == globals.benpassant ) )
	return 1;
    
    return 0;
}

int ncheck(move m)
{
    int hor = getx(gets2(m)) - getx(gets1(m));
    int ver = gety(gets2(m)) - gety(gets1(m));
    
    if ( (abs(hor) == 2) && ( abs(ver) == 1))
	return 1;
    if ( (abs(ver) == 2) && ( abs(hor) == 1))
	return 1;
    return 0;
}

int bcheck(move m)
{ 
    int hor = getx(gets2(m)) - getx(gets1(m));
    int ver = gety(gets2(m)) - gety(gets1(m));
    
    if ( ( ( hor == ver ) || ( hor == -ver ) ) &&
	 ( clearpathp (gets1(m), gets2(m)) ))
	return 1;
    return 0;
}

int rcheck(move m)
{
    int hor = getx(gets2(m)) - getx(gets1(m));
    int ver = gety(gets2(m)) - gety(gets1(m));
    
    if ( (hor == 0) && (clearpathp(gets1(m), gets2(m)) ) )
	return 1;

    if ( (ver == 0) && (clearpathp(gets1(m), gets2(m)) ) )
	return 1;
    
    return 0;
}

int qcheck(move m)
{
    return (rcheck(m)||bcheck(m));
}

int kcheck(move m)
{
    int hor = getx(gets2(m)) - getx(gets1(m));
    int ver = gety(gets2(m)) - gety(gets1(m));

    if ( ( abs(hor) <= 1 ) && ( abs(ver) <= 1 ) )
	return 1;

    /* white o-o */
    if ( (moveequal ( m , whiteoo )) &&
	 (clearpathp ( gets1(m), makesquare(7,0))) &&
	 (globals.wcanoo) &&
	 (!incheckp (WHITE) ) &&
	 (!checkchecker( wthruoo, WHITE)) )
	return 1;

    /* white o-o-o */
    if ( (moveequal ( m , whiteooo )) &&
	 (clearpathp ( gets1(m), makesquare(0,0) )) &&
	 (globals.wcanooo) &&
	 (!incheckp (WHITE) ) &&
	 (!checkchecker( wthruooo, WHITE)) )
	return 1;

    /* black o-o */
    if ( (moveequal ( m , blackoo )) &&
	 (clearpathp ( gets1(m), makesquare(7,7) )) &&
	 (globals.bcanoo) &&
	 (!incheckp (BLACK) ) &&
	 (!checkchecker( bthruoo, BLACK)) )
	return 1;

    /* black o-o-o */
    if ( (moveequal ( m , blackooo )) &&
	 (clearpathp ( gets1(m), makesquare(0,7) )) &&
	 (globals.bcanooo) &&
	 (!incheckp (BLACK) ) &&
	 (!checkchecker( bthruooo, BLACK )) )
	return 1;
	 
    return 0;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int checkmatep(int color)
{
    return (incheckp(color) && (mobility(color)==0));
}

int stalematep(int color)
{
    return (!incheckp(color) && (mobility(color)==0));
}

int gameoverp ()
{
    movelist *list;
    
    if ( globals. turn == WHITE  ) 
    {
	if (checkmatep (WHITE))
	{
	    list = validmoves(WHITE);
	    if (list != NULL)
	    {
		deletelist(list);
		return 0;
	    }

	    printf ("white checkmated\n");
	    return 1;
	}
	if (stalematep (WHITE))
	{
	    list = validmoves(WHITE);
	    if (list != NULL)
	    {
		deletelist(list);
		return 0;
	    }
	    
	    printf ("white stalemated\n");
	    return 1;
	}
	if (incheckp (WHITE))
	{
	    printf ("white in check\n");
	}
    }
    if ( globals.turn == BLACK)
    {
	if (checkmatep (BLACK))
	{
	    list = validmoves(BLACK);
	    if (list != NULL)
	    {
		deletelist(list);
		return 0;
	    }
	    
	    printf("black checkmated\n");
	    return 1;
	}
	if (stalematep (BLACK))
	{
	    list = validmoves(BLACK);
	    if (list != NULL)
	    {
		deletelist(list);
		return 0;
	    }
	    
	    printf("black stalemated\n");
	    return 1;
	}
	if (incheckp (BLACK) )
	{
	    printf("black in check\n");
	}
    }
    
    return 0;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
movelist *pmoves(square loc)
{
    int color = getcolor(getxy(getx(loc), gety(loc)));
    int x = getx(loc);
    int direction = color;
    int y = gety(loc);
    int start = (color == WHITE) ? 1 : 6;
    movelist *res = newlist();

    if (getxy(x, y + direction) == empty)
    {
	push(&res, makemove(loc, makesquare(x, y+direction)));
    }
        
    if ((getxy(x, y+2*direction) == empty) &&
	(y == start)&&
	(getxy(x, y+direction) == empty))
    {
	push(&res, makemove(loc, makesquare(x,y+2*direction)));
    }
    
    if (getcolor (getxy (x+1, y+direction)) == -color)
	push(&res, makemove(loc, makesquare(x+1,y+direction)));

    if (getcolor (getxy (x-1, y+direction)) == -color)
	push(&res, makemove(loc, makesquare(x-1,y+direction)));

    if ( (y+1==5) &&
	 ( color == WHITE ) &&
	 ( x+1 == globals.wenpassant ))
	push(&res, makemove(loc, makesquare(x+1,y+direction)));

    if (( y-1 == 2 ) &&
	( color == BLACK ) &&
	( x-1 == globals.benpassant ) )
	push(&res, makemove(loc, makesquare(x-1,y+direction)));
    
    return res;
}

movelist *nmoves(square loc)
{
    movelist *list = newlist();
    int x = getx(loc);
    int y = gety(loc);
    int i;
    int x2,y2,color=getcolor(getxy(x,y));
    move m;
    
    for (i=0;i<4;i++)
    {
	x2 = x+knightdx[i];
	y2 = y+knightdy[i];
	if (!offboardp(x2,y2))
	{
	    m = makemove(loc, makesquare(x2,y2));
	    if (getcolor(getxy(x2,y2))!=color&&
		!checkchecker(m,color))
		push(&list, makemove ( loc, makesquare ( x2, y2 ) ) );

	}

	x2 = x+knightdy[i];
	y2 = y+knightdx[i];
	if (!offboardp(x2,y2))
	{
	    m = makemove(loc, makesquare(x2,y2));
	    if (getcolor(getxy(x2,y2))!=color&&
		!checkchecker(m,color))
		push(&list, makemove ( loc, makesquare ( x2, y2 ) ) );
	}
    }
        
    return list;
}

movelist *bmoves(square loc)
{
    movelist *list = newlist();
    int s1, s2, s3, s4;
    int x, y, dx, dy;
    int color = getcolor(getxy(getx(loc),gety(loc)));
    int c1, c2, c3, c4;
    
    s1 = s2 = s3 = s4 = 1;
    x = getx(loc);
    y = gety(loc);
    
    for (dx = 1, dy = 1; ; dx++, dy++)
    {
	if (offboardp (x + dx, y + dy))
	    s1 = 0;
	if (offboardp (x + dx, y - dy))
	    s2 = 0;
	if (offboardp (x - dx, y + dy))
	    s3 = 0;
	if (offboardp (x - dx, y - dy))
	    s4 = 0;

	if ( (s1 == s2) && (s2 == s3) && (s3 == s4) && (s4 == 0) )
	    break;

	c1 = getcolor(getxy(x+dx, y+dy));
	c2 = getcolor(getxy(x+dx, y-dy));
	c3 = getcolor(getxy(x-dx, y+dy));
	c4 = getcolor(getxy(x-dx, y-dy));
	
	if (s1)
	{
	    if (c1 == color)
		s1 = 0;
	    else if (c1 == 0)
		push(&list, makemove(loc, makesquare(x+dx,y+dy)));
	    else if (c1 == -color)
	    {
		push(&list, makemove(loc, makesquare(x+dx,y+dy)));
		s1 = 0;
	    }
	}

	if (s2)
	{
	    if (c2 == color)
		s2 = 0;
	    else if (c2 == 0)
		push(&list, makemove(loc, makesquare(x+dx,y-dy)));
	    else if (c2 == -color)
	    {
		push(&list, makemove(loc, makesquare(x+dx,y-dy)));
		s2 = 0;
	    }
	}

	if (s3)
	{
	    if (c3 == color)
		s3 = 0;
	    else if (c3 == 0)
		push(&list, makemove(loc, makesquare(x-dx,y+dy)));
	    else if (c3 == -color)
	    {
		push(&list, makemove(loc, makesquare(x-dx,y+dy)));
		s3 = 0;
	    }
	}

	if (s4)
	{
	    if (c4 == color)
		s4 = 0;
	    else if (c4 == 0)
		push(&list, makemove(loc, makesquare(x-dx,y-dy)));
	    else if (c4 == -color)
	    {
		push(&list, makemove(loc, makesquare(x-dx,y-dy)));
		s4 = 0;
	    }
	}
    }
    return list;
}

movelist *rmoves(square loc)
{
    movelist *list = newlist();
    int s1, s2, s3, s4;
    int x, y, dx, dy;
    int color = getcolor(getxy(getx(loc),gety(loc)));
    int c1, c2, c3, c4;
    
    s1 = s2 = s3 = s4 = 1;
    x = getx(loc);
    y = gety(loc);
    
    for (dx = 1, dy = 1; ; dx++, dy++)
    {
	if (offboardp (x + dx, y))
	    s1 = 0;
	if (offboardp (x - dx, y))
	    s2 = 0;
	if (offboardp (x, y + dy))
	    s3 = 0;
	if (offboardp (x, y - dy))
	    s4 = 0;

	if ( (s1 == s2) && (s2 == s3) && (s3 == s4) && (s4 == 0) )
	    break;

	c1 = getcolor(getxy(x+dx, y));
	c2 = getcolor(getxy(x-dx, y));
	c3 = getcolor(getxy(x, y+dy));
	c4 = getcolor(getxy(x, y-dy));
	
	if (s1)
	{
	    if (c1 == color)
		s1 = 0;
	    else if (c1 == 0)
		push(&list, makemove(loc, makesquare(x+dx,y)));
	    else if (c1 == -color)
	    {
		push(&list, makemove(loc, makesquare(x+dx,y)));
		s1 = 0;
	    }
	}

	if (s2)
	{
	    if (c2 == color)
		s2 = 0;
	    else if (c2 == 0)
		push(&list, makemove(loc, makesquare(x-dx,y)));
	    else if (c2 == -color)
	    {
		push(&list, makemove(loc, makesquare(x-dx,y)));
		s2 = 0;
	    }
	}

	if (s3)
	{
	    if (c3 == color)
		s3 = 0;
	    else if (c3 == 0)
		push(&list, makemove(loc, makesquare(x,y+dy)));
	    else if (c3 == -color)
	    {
		push(&list, makemove(loc, makesquare(x,y+dy)));
		s3 = 0;
	    }
	}

	if (s4)
	{
	    if (c4 == color)
		s4 = 0;
	    else if (c4 == 0)
		push(&list, makemove(loc, makesquare(x,y-dy)));
	    else if (c4 == -color)
	    {
		push(&list, makemove(loc, makesquare(x,y-dy)));
		s4 = 0;
	    }
	}
	
    }
    return list;
}

movelist *qmoves(square loc)
{
    movelist *list1, *list2;
    
    list1 = bmoves(loc);
    list2 = rmoves(loc);

    while (list2)
    {
	push(&list1, pop(&list2));
    }

    return list1;
}
   
movelist *kmoves(square loc)
{
    int color = getcolor(getxy(getx(loc), gety(loc)));
    movelist *list = newlist();
    int x = getx(loc);
    int y = gety(loc);

    int dx, dy;
    move m;
    
    for (dx=-1;dx<=1;dx++)
    {
	for (dy=-1;dy<=1;dy++)
	{
	    if (x+dx==y+dy) continue;
	    m = makemove(loc, makesquare(x+dx,y+dy));
	    if (!offboardp(x+dx,y+dy)&&
		!checkchecker(m,color)&&
		getcolor(getxy(x+dx,y+dy))!=color)
		push(&list, m);
	}
    }

    if ((color==WHITE)&&squareequal(loc, gets1(whiteoo)))
    {
	if (globals.wcanoo&&
	    !incheckp(WHITE)&&
	    !checkchecker( wthruoo, WHITE)&&
	    clearpathp ( loc, makesquare(7,0)))
	    push(&list, whiteoo);

	if (globals.wcanooo&&
	    !incheckp(WHITE)&&
	    !checkchecker( wthruooo, WHITE)&&
	    clearpathp ( loc, makesquare(0,0)))
	    push(&list, whiteooo);
    }

    if ((color==BLACK)&&squareequal(loc, gets1(blackoo)))
    {
	if (globals.bcanoo&&
	    clearpathp ( loc, makesquare(7,7)) &&
	    !incheckp(BLACK)&&
	    !checkchecker( bthruoo, BLACK ))
	    push(&list, blackoo);

	if (globals.bcanooo&&
	    clearpathp ( loc, makesquare(0,7) ) &&
	    !incheckp(BLACK)&&
	    !checkchecker( bthruooo, BLACK ))
	    push(&list, blackooo);
    }
    
    return list;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int pmovecount(square loc)
{
    int color = getcolor(getxy(getx(loc), gety(loc)));
    int x = getx(loc);
    int direction = color;
    int y = gety(loc);
    int start = (color == WHITE) ? 1 : 6;
    int res = 0;
    move m;
    
    m=makemove(loc, makesquare(x,y+direction));
    if ((getxy(x, y + direction) == empty)&&
	!checkchecker(m,color))
    {
	res ++;
    }

    m=makemove(loc, makesquare(x,y+2*direction));
    if ((getxy(x, y+2*direction) == empty) &&
	(y == start)&&
	(getxy(x, y+direction) == empty)&&
	!checkchecker(m,color))
    {
	res ++;
    }

    if (!offboardp(x+1,y+direction))
    {
	m=makemove(loc, makesquare(x+1,y+direction));
	if ((getcolor (getxy (x+1, y+direction)) == -color)&&
	    !checkchecker(m,color))
	    res ++;
    }
    
    if (!offboardp(x-1,y+direction))
    {
	m=makemove(loc, makesquare(x-1,y+direction));
	if ((getcolor (getxy (x-1, y+direction)) == -color)&&
	    !checkchecker(m,color))
	    res ++;
    }
    
    if (!offboardp(x+1,y+1))
    {
	m=makemove(loc, makesquare(x+1,y+1));
	if ( (y+1==5) &&
	     ( color == WHITE ) &&
	     ( x+1 == globals.wenpassant )&&
	     !checkchecker(m,color))
	    res ++;
    }
    
    if (!offboardp(x-1,y-1))
    {
	m=makemove(loc, makesquare(x+1,y+1));
	if (( y-1 == 2 ) &&
	    ( color == BLACK ) &&
	    ( x-1 == globals.benpassant )&&
	    !checkchecker(m,color))
	    res ++;
    }
    
    return res;
}

int nmovecount(square loc)
{
    int res = 0;
    int i;
    
    int x = getx(loc);
    int y = gety(loc);

    int color=getcolor(getxy(x,y));
    move m;
    int x2, y2;

    for (i=0;i<4;i++)
    {
	x2 = x+knightdx[i];
	y2 = y+knightdy[i];
	if (!offboardp(x2,y2))
	{
	    m = makemove(loc, makesquare(x2,y2));
	    if (getcolor(getxy(x2,y2))!=color&&
		!checkchecker(m,color))
		res ++;
	}

	x2 = x+knightdy[i];
	y2 = y+knightdx[i];
	if (!offboardp(x2,y2))
	{
	    m = makemove(loc, makesquare(x2,y2));
	    if (getcolor(getxy(x2,y2))!=color&&
		!checkchecker(m,color))
		res ++;
	}
    }
    return res;
}

int bmovecount(square loc)
{
    int s1, s2, s3, s4;
    int x, y, dx, dy;
    int color = getcolor(getxy(getx(loc),gety(loc)));
    int c1, c2, c3, c4;
    int res = 0;
    move m;
    
    s1 = s2 = s3 = s4 = 1;
    x = getx(loc);
    y = gety(loc);
    
    for (dx = 1, dy = 1; ; dx++, dy++)
    {
	m = makemove(loc, makesquare(x+dx,y+dy));
	if (offboardp (x + dx, y + dy)||
	    checkchecker(m,color))
	    s1 = 0;

	m = makemove(loc, makesquare(x+dx,y-dy));
	if (offboardp (x + dx, y - dy)||
	    checkchecker(m,color))
	    s2 = 0;

	m = makemove(loc, makesquare(x-dx,y+dy));
	if (offboardp (x - dx, y + dy)||
	    checkchecker(m,color))					   
	    s3 = 0;

	m = makemove(loc, makesquare(x-dx,y-dy));
	if (offboardp (x - dx, y - dy)||
	    checkchecker(m,color))
	    s4 = 0;
	
	if ((s1 == 0) && (s1 == s2) && (s2 == s3) && (s3 == s4))
	    break;

	c1 = getcolor(getxy(x+dx, y+dy));
	c2 = getcolor(getxy(x+dx, y-dy));
	c3 = getcolor(getxy(x-dx, y+dy));
	c4 = getcolor(getxy(x-dx, y-dy));
	
	if (s1)
	{
	    if (c1 == color)
		s1 = 0;
	    else if (c1 == 0)
		res ++;
	    else if (c1 == -color)
	    {
		res++;
		s1 = 0;
	    }
	}

	if (s2)
	{
	    if (c2 == color)
		s2 = 0;
	    else if (c2 == 0)
		res++;
	    else if (c2 == -color)
	    {
		res++;
		s2 = 0;
	    }
	}

	if (s3)
	{
	    if (c3 == color)
		s3 = 0;
	    else if (c3 == 0)
		res++;
	    else if (c3 == -color)
	    {
		res++;
		s3 = 0;
	    }
	}

	if (s4)
	{
	    if (c4 == color)
		s4 = 0;
	    else if (c4 == 0)
		res++;
	    else if (c4 == -color)
	    {
		res++;
		s4 = 0;
	    }
	}
    }
    return res;
}

int rmovecount(square loc)
{
    int s1, s2, s3, s4;
    int x, y, dx, dy;
    int color = getcolor(getxy(getx(loc),gety(loc)));
    int c1, c2, c3, c4;
    int res = 0;
    move m;
    
    s1 = s2 = s3 = s4 = 1;
    x = getx(loc);
    y = gety(loc);
    
    for (dx = 1, dy = 1; ; dx++, dy++)
    {
	m = makemove(loc, makesquare(x+dx,y));
	if ((offboardp (x + dx, y))||
	    checkchecker(m,color))
	    s1 = 0;

	m = makemove(loc, makesquare(x-dx,y));
	if ((offboardp (x - dx, y))||
	    checkchecker(m,color))
	    s2 = 0;
	    
	m = makemove(loc, makesquare(x,y+dy));
	if ((offboardp (x, y + dy))||
	    checkchecker(m,color))
	    s3 = 0;

	m = makemove(loc, makesquare(x,y-dy));
	if ((offboardp (x, y - dy))||
	    checkchecker(m,color))
	    s4 = 0;

	if ( (s1 == 0) && (s1 == s2) && (s2 == s3) && (s3 == s4) )
	    break;

	c1 = getcolor(getxy(x+dx, y));
	c2 = getcolor(getxy(x-dx, y));
	c3 = getcolor(getxy(x, y+dy));
	c4 = getcolor(getxy(x, y-dy));
	
	if (s1)
	{
	    if (c1 == color)
		s1 = 0;
	    else if (c1 == 0)
		res++;
	    
	    else if (c1 == -color)
	    {
		res++;
		
		s1 = 0;
	    }
	}

	if (s2)
	{
	    if (c2 == color)
		s2 = 0;
	    else if (c2 == 0)
		res++;
	    
	    else if (c2 == -color)
	    {
		res++;
		
		s2 = 0;
	    }
	}

	if (s3)
	{
	    if (c3 == color)
		s3 = 0;
	    else if (c3 == 0)
		res++;
	    
	    else if (c3 == -color)
	    {
		res++;
		
		s3 = 0;
	    }
	}

	if (s4)
	{
	    if (c4 == color)
		s4 = 0;
	    else if (c4 == 0)
		res++;
	    
	    else if (c4 == -color)
	    {
		res++;
		s4 = 0;
	    }
	}
	
    }
    return res;
}

int qmovecount(square loc)
{
    return bmovecount(loc)+rmovecount(loc);
}

int kmovecount(square loc)
{
    int color = getcolor(getxy(getx(loc), gety(loc)));
    int x = getx(loc);
    int y = gety(loc);
    int dx, dy;
    int res=0;
    move m;
    
    for (dx=-1;dx<=1;dx++)
    {
	for (dy=-1;dy<=1;dy++)
	{
	    if (x+dx==y+dy) continue;
	    m = makemove(loc, makesquare(x+dx,y+dy));
	    if (!offboardp(x+dx,y+dy)&&
		!checkchecker(m,color)&&
		getcolor(getxy(x+dx,y+dy))!=color)
		res++;
	}
    }

    if ((color==WHITE)&&squareequal(loc, gets1(whiteoo)))
    {
	if (globals.wcanoo&&
	    !incheckp(WHITE)&&
	    !checkchecker( wthruoo, WHITE )&&
	    clearpathp ( loc, makesquare(7,0)))
	    res ++;
	
	if (globals.wcanooo&&
	    !incheckp(WHITE)&&
	    !checkchecker( wthruooo, WHITE)&&
	    clearpathp ( loc, makesquare(0,0)))
	    res++;
    }

    if ((color==BLACK)&&squareequal(loc, gets1(blackoo)))
    {
	if (globals.bcanoo&&
	    clearpathp ( loc, makesquare(7,7) ) &&
	    !incheckp(BLACK)&&
	    !checkchecker( bthruoo, BLACK))
	    res++;

	if (globals.bcanooo&&
	    clearpathp ( loc, makesquare(0,7) ) &&
	    !incheckp(BLACK)&&
	    !checkchecker( bthruooo, BLACK ))
	    res++;
    }
    return res;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int pawnsetup(int color)
{
    int setup[8] = {0,0,0,0,0,0,0,0};
    piece pawn;
    int start;
    int doubled=0;
    int unmoved=0;
    int isolated=0;
    int x;
    int count;
    
    start = (color == WHITE) ? 1 : 6;
    pawn = (color == WHITE) ? wp : bp;
    
    loopboard
	{
	    if (getxy(x,y)==pawn)
	    {
		if (y==start)
		    unmoved++;
		setup[x]++;
	    }
	}
    endloop;

    for (x=0;x<8;x++)
    {
	count = setup[x];

	if (count >0)
	{
	    if (x==0)
	    {
		if (setup[x+1]==0)
		    isolated++;
	    }
	    
	    else if (x==7)
	    {
		if (setup[x-1]==0)
		    isolated++;
	    }
	    
	    else		/* if ((x>0) && (x<7))*/
	    {
		if ((setup[x-1]==0) && (setup[x+1]==0))
		    isolated++;
	    }
	    
	    doubled+=count-1;
	}
    }
    return isolated+doubled+unmoved;
}

int pawnpromo(int color)
{
    piece pawn;
    int start, goal;
    int res=0;
    
    if (color==WHITE)
    {
	pawn = wp;
	start=1;
	goal=7;
    }
    else
    {
	pawn = bp;
	start=6;
	goal=0;
    }
    
    loopboard
	{
	    if (getxy(x,y)==pawn)
	    {
		res += 1 << ( 4 * (7-abs(goal-start)));
	    }
	}
    endloop;
    return (color==WHITE) ? res : -res;
}

int material ()
{
    int res = 0;
    
    loopboard
	{
	    res += piecevalues[getxy(x,y)];
	}
    endloop;
    return res;
}

int mobility(int color)
{
    int res=0;
    piece p;
    square sq;
    
    loopboard
	{
	    p = getxy(x,y);
	    if (getcolor(p)==color)
	    {
		sq = makesquare(x,y);

		switch(p)
		{
		    case wp: case bp:
			res += pmovecount(sq);
			break;
		    case wn: case bn:
			res += nmovecount(sq);
			break;
		    case wb: case bb:
			res += bmovecount(sq);
			break;
		    case wr: case br:
			res += rmovecount(sq);
			break;
		    case wq: case bq:
			res += qmovecount(sq);
			break;
		    case wk: case bk:
			res += kmovecount(sq);
			break;
		    case empty:break;
		}
	    }
	}
    endloop;

    if (color==BLACK) res=-res;
    return res;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
movelist *validmoves(int color)
{
    movelist *list, *tmp = NULL;
    movelist *locs;
    square sq;
    piece p;
    move m;
    int x2,y2;
    
    list = newlist();
    locs = locations(color);

    while(locs)
    {
	m = pop(&locs);
	sq = gets1(m);
	p = getxy(getx(sq),gety(sq));

	switch(p)
	{
	    case wp: case bp:
		tmp = pmoves(sq);
		break;
	    case wn: case bn:
		tmp = nmoves(sq);
		break;
	    case wb: case bb:
		tmp = bmoves(sq);
		break;
	    case wr: case br:
		tmp = rmoves(sq);
		break;
	    case wq: case bq:
		tmp = qmoves(sq);
		break;
	    case wk: case bk:
		tmp = kmoves(sq);
		break;
	    case empty:
		tmp = newlist();
		break;
	}

	while (tmp)
	{
	    m = pop(&tmp);
	    
	    x2 = getx(gets2(m));
	    y2 = gety(gets2(m));
	    
	    if ( /*!offboardp ( x2, y2) &&*/
		 !checkchecker(m, color)
		 /*&&
		 (color != getcolor(getxy( x2, y2)))
		 */)
	    {
		push(&list, m);
	    }
	}
    }
    return list;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int checkchecker (move m, int color)
{
    struct variables v;
    movelist *list;
    int res = 0;
    int savedplayer[2];

    savedplayer[0]=computercontrolled[0];
    savedplayer[1]=computercontrolled[1];
    computercontrolled[0]=computercontrolled[1]=1;

    list = domove (m, &v);
    
    if ( incheckp( color) ) res = 1;

    undomove (&list);
    restorevariables(v); 

    computercontrolled[0]=savedplayer[0];
    computercontrolled[1]=savedplayer[1];
    
    return res;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
square findking(int color)
{
    return (color == WHITE) ? wking : bking;
}

movelist *locations(int color)
{
    movelist *res = newlist();
    move m;
    piece p;
    loopboard
	{
	    p = getxy(x,y);
	    if ( getcolor (p) == color)
	    {
		m = makemove(makesquare(x,y), makesquare(x,y));
		setpiece(&m,p);
		push (&res, m);
	    }
	}
    endloop;

    return res;
}

movelist *pieces(int color)
{
    return locations(color);
}
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int incheckp (int color)
{
    square ksq;
    square oksq;
    
    piece knight;
    piece bishop;
    piece rook;  
    piece queen; 
    piece pawn;

    int x, y;
    int i;
    int dx, dy;
    int x2, y2;
    int c1,c2,c3,c4;
    
    piece p;
    
    ksq = findking (color);

    x = getx(ksq);
    y = gety(ksq);

    oksq = findking (-color);
    
    x2 = getx(oksq);
    y2 = gety(oksq);
    
    if ((abs(x2-x)<=1)&&(abs(y2-y)<=1))
	return 1;
    
    if (color==WHITE)
    {
	knight=bn;
	bishop=bb;
	queen=bq;
	rook=br;
	pawn=bp;

	if (!offboardp(x-1,y+1) && (getxy(x-1,y+1)==pawn))
	    return 1;

	if (!offboardp(x+1,y+1) && (getxy(x+1,y+1)==pawn))
	    return 1;
    }

    else /* (color==BLACK) */
    {
	knight=wn;
	bishop=wb;
	queen=wq;
	rook=wr;
	pawn=wp;
	
	if (!offboardp(x-1,y-1) && (getxy(x-1,y-1)==pawn))
	    return 1;
	    
	if (!offboardp(x+1,y-1) && (getxy(x+1,y-1)==pawn))
	    return 1;
    }
    
    for (i=0;i<4;i++)
    {
	if (!offboardp(x+knightdx[i],y+knightdy[i]) &&
	    (getxy(x+knightdx[i],y+knightdy[i]) == knight))
	    return 1;

	if (!offboardp(x+knightdy[i],y+knightdx[i]) &&
	    (getxy(x+knightdy[i],y+knightdx[i]) == knight))
	    return 1;
    }

    /* can a bishop get em */

    for (dx=1,dy=1;(x+dx<8)&&(y+dy<8);dx++,dy++)
    {
	p = getxy(x+dx,y+dy);
	if (p)
	{
	    if (p==queen || p==bishop)
		return 1;
	    break;
	}
    }
    
    for (dx=-1,dy=-1;(x+dx>=0)&&(y+dy>=0);dx--,dy--)
    {
	p = getxy(x+dx,y+dy);
	if (p)
	{
	    if (p==queen||p==bishop)
		return 1;
	    break;
	}
    }

    for (dx=1,dy=-1;(x+dx<=7)&&(y+dy>=0);dx++,dy--)
    {
	p = getxy(x+dx,y+dy);
	if (p)
	{
	    if (p==queen||p==bishop)
		return 1;
	    break;
	}
    }

    for (dx=-1,dy=1;(x+dx>=0)&&(y+dy<=7);dx--,dy++)
    {
	p = getxy(x+dx,y+dy);
	if (p)
	{
	    if (p==queen||p==bishop)
		return 1;
	    break;
	}
    }

    
    /* can a rook get em */
    c1 = 1;
    c2 = 1;
    c3 = 1;
    c4 = 1;

    for (i=1; ; i++)
    {
	if (x+i>7)
	    c1 = 0;

	if (x-i<0)
	    c2 = 0;

	if (y-i<0)
	    c3 = 0;

	if (y+i>7)
	    c4 = 0;
	
	if ( (c1 == 0) && (c1 == c2) && (c2 == c3) && (c3 == c4) )
	    break;

	if (c1)
	{
	    p = getxy(x+i,y);
	    if (p)
	    {
		if ((p == rook) || (p == queen))
		    return 1;
		c1 = 0;
	    }
	}
	
	if (c2)
	{
	    p = getxy(x-i,y);
	    if (p)
	    {
		if ((p == rook) || (p == queen))
		    return 1;
		c2 = 0;
	    }
	}
	
	if (c3)
	{
	    p = getxy(x,y-i);
	    if (p)
	    {
		if ((p == rook) || (p == queen))
		    return 1;
		c3 = 0;
	    }
	}

	if (c4)
	{
	    p = getxy(x,y+i);
	    if (p)
	    {
		if ((p == rook) || (p == queen))
		    return 1;
		c4 = 0;
	    }
	}
    }

    return 0;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int validp(move m)
{
    return isvalid(m, 1);
}

int validp_nocheck(move m)
{
    return isvalid(m, 0);
}

    
int isvalid(move m,int check)
{
    piece p;
    int color;
   

    p = getxy(getx(gets1(m)), gety(gets1(m)));
    
    color = getcolor(p);
    
    /* printf("piece = %d\ncolor = %d\n", p, color); */
    
    if (offboardp( getx(gets2(m)), gety(gets2(m))))
	return 0;

    if (getcolor ( getxy(getx(gets2(m)),gety(gets2(m)))) == color)
	return 0;
    
    switch ( getxy ( getx(gets1(m)), gety(gets1(m))) )
    {
	case wp: case bp:
	    if (! pcheck ( m ) )
		return 0;
	    break;
	case wn: case bn:
	    if (! ncheck ( m ) )
		return 0;
	    break;
	case wb: case bb:
	    if (! bcheck ( m ) )
		return 0;
	    break;
	case wr: case br:
	    if (! rcheck ( m ) )
		return 0;
	    break;
	case wq: case bq:
	    if (! qcheck ( m ) )
		return 0;
	    break;
	case wk: case bk:
	    if (! kcheck ( m ) )
		return 0;
	    break;
	default:
	    return 0;
    }

    if (check && checkchecker (m, color) )
	return 0;
    
    return 1;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
move getmove()
{
    char a,b;
    int y1,y2;
    move m;
    char string[10];
    
    gets(string);

    if (!strcmp(string, "exit"))
	return NILMOVE;

    if (!strcmp(string, "draw"))
	return DRAWREP;
    
    sscanf (string, "%c%d%c%d", &a, &y1, &b, &y2);

    m = makemove(makesquare(a-'a', y1-1),
		 makesquare(b-'a', y2-1));
    
    return m;
}

piece choosepiece(int color)
{
    char ch = 0;
    piece p = empty;
    char s[5];
    
    if (computercontrolled[0]&&(color == WHITE)) return wq;
    if (computercontrolled[1]&&(color == BLACK)) return bq;

    while(!p)
    {
	printf("Q, R, B, N : ");
	gets(s);
	sscanf(s, "%c", &ch);
	if (color==WHITE)
	{
	    switch(ch)
	    {
		case 'b':case 'B':
		    p = wb;
		    break;
		case 'n':case 'N':
		    p = wn;
		    break;
		case 'r':case 'R':
		    p = wr;
		    break;
		case 'q':case 'Q':
		    p = wq;
		    break;
	    }
	}
	else
	{
	    switch(ch)
	    {
		case 'b':case 'B':
		    p = bb;
		    break;
		case 'n':case 'N':
		    p = bn;
		    break;
		case 'r':case 'R':
		    p = br;
		    break;
		case 'q':case 'Q':
		    p = bq;
		    break;
	    }
	}
    }
    return p;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
struct variables savevariables()
{
    return globals;
}

void printvariables (struct variables v)
{
    printf(" (%d, %d), (%d, %d), %d, %d, %d\n",
	   v.wcanoo, v.wcanooo,
	   v.bcanoo, v.bcanooo,
	   v.wenpassant, v.benpassant, v.turn);
}

void restorevariables(struct variables v)
{
    globals = v;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
movelist *domove(move m, struct variables *oldv)
{
    int x1 = getx(gets1(m));
    int x2 = getx(gets2(m));
    int y1 = gety(gets1(m));
    int y2 = gety(gets2(m));
    
    piece p = getxy(x1, y1);
    movelist *undo;
    movelist *moreundo = NULL;
    struct variables v;
    move m1, m2;
    move tmpmove;

    int clearw, clearb;

    int hor, ver;
    square s1 = gets1(m);

    clearw = clearb = 0;

    movecount++;
    
    hor = x2 - x1;
    ver = y2 - y1;

    v = savevariables();
    
    m1 = makemove(gets1(m), makesquare( 8, 8));
    m2 = makemove(gets2(m), makesquare( 8, 8));
    
    setpiece(&m1, p);
    setpiece(&m2, getxy(x2, y2));

    undo = newlist();
    push ( &undo, m1);
    push ( &undo, m2);
    
    setxy(x2, y2, p);
    setxy(x1, y1, empty);

    if (globals.benpassant<8)
	clearb=1;
    if (globals.wenpassant<8)
	clearw=1;

    switch(p)
    {
	case wp:
	    if (y2 == 7)
		setxy(x2,y2,choosepiece(WHITE));

	    else if ( ( x2 == globals.wenpassant) &&
		      ( ver == 1 ) &&
		      ( ( hor == 1) || (hor == -1) ) &&
		      ( y2 == 5 ))
	    {
		tmpmove = makemove(makesquare(x2, y2-1), makesquare(8,8));
		setpiece(&tmpmove, bp);
		push (&undo, tmpmove);
		setxy (x2, y2-1,empty);
	    }

	    else if (ver == 2)
		globals.benpassant = x1;

	    break;

	case bp:
	    if (y2 == 0)
		setxy(x2,y2,choosepiece(BLACK));

	    else if ( ( x2 == globals.benpassant ) &&
		      (ver == -1) &&
		      ( (hor == 1) || (hor == -1) ) &&
		      ( y2 == 2 ))
	    {
		tmpmove = makemove(makesquare (x2, y2+1), makesquare(8,8));
		setpiece(&tmpmove, bp);
		push ( &undo, tmpmove );
		setxy( x2, y2 + 1, empty);
	    }
	    else if (ver == -2)
		globals.wenpassant = x1;
	    
	    break;

	case wk:
	    if (moveequal(m, whiteoo))
	    {
		tmpmove = makemove(makesquare (7,0), makesquare(5,0) );
		moreundo = domove( tmpmove, &v);
		globals.wcanoo = globals.wcanooo = 0;
	    }
	    else if (moveequal(m, whiteooo) )
	    {
		tmpmove = makemove(makesquare (0,0), makesquare(3,0) );
		moreundo = domove( tmpmove, &v);
		globals.wcanoo = globals.wcanooo = 0;
	    }
	    globals.wcanoo = globals.wcanooo = 0;
	    wking = gets2(m);
	    break;

	case bk:
	    if (moveequal(m, blackoo))
	    {
		tmpmove = makemove(makesquare (7,7), makesquare(5,7) );
		moreundo = domove( tmpmove, &v);
		globals.bcanoo = globals.bcanooo = 0;
	    }
	    else if (moveequal(m, blackooo) )
	    {
		tmpmove = makemove(makesquare (0,7), makesquare(3,7) );
		moreundo = domove( tmpmove, &v);
		globals.bcanoo = globals.bcanooo = 0;
	    }
	    globals.bcanoo = globals.bcanooo = 0;	    
	    bking = gets2(m);
	    break;

	case wr:
	    if (moveequal(s1, A1))
		globals.wcanoo = 0;
	    if (moveequal(s1, G1))
		globals.wcanooo = 0;
	    break;

	case br:
	    if (moveequal(s1, A8))
		globals.bcanoo = 0;
	    if (moveequal(s1, G8))
		globals.bcanooo = 0;
	    break;

	case empty:
	case bn:case wn:
	case bb:case wb:
	case bq:case wq:
	    /* no special moves */
	    break;
	    
    }

    while (moreundo)
    {
	push(&undo, pop(&moreundo));
    }

    *oldv = v;

    if (clearw)
	globals.wenpassant = 8;
    if (clearb)
	globals.benpassant = 8;

    return undo;
}

void undomove(movelist **list)
{
    move m;
    piece p;
    
    while (*list)
    {
	m = pop (list);
	p = getpiece(m);
	setxy(getx(gets1(m)), gety(gets1(m)), p);

	if (p==wk)
	    wking = gets1(m);
	if (p==bk)
	    bking = gets1(m);
    }
}
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
int evalboard(int color)
{
    int res;
    int mat;
    int mob;
    int pawns;
    
    mat = material ();
    
    mob = mobility(WHITE)+mobility(BLACK);

    pawns = pawnsetup(WHITE)-pawnsetup(BLACK);

    res = mat * 5 + pawns * 2 + mob /*+ pawnpromo(color)*/;

    if (checkmatep(-color))
    {
	res = CHECKMATE * color;
    }
    
    if ((res < 0) && (color == WHITE) && stalematep(color))
    {
	res = STALEMATE;
    }

    if ((res > 0) && (color == BLACK) && stalematep(color))
    {
	res = -STALEMATE;
    }

    return res;
}

/* to play around with and see what the better evaluator is */
int evalboard2(int color)
{
    int res;
    int mat;
    int mob;
    int pawns;
    
    mat = material ();
    
    mob = mobility(WHITE)+mobility(BLACK);

    pawns = pawnsetup(WHITE)-pawnsetup(BLACK);

    res = mat * 5 + pawns * 10 + mob;

    if (checkmatep(-color))
    {
	res = CHECKMATE * color;
    }
    
    if ((res < 0) && (color == WHITE) && stalematep(color ))
    {
	res = STALEMATE;
    }

    if ((res > 0) && (color == BLACK) && stalematep(color))
    {
	res = -STALEMATE;
    }

    return res;
}

int evalboard3(int color)
{
    int res;
    int mat;
    int mob;
    int pawns;
    
    mat = material ();
    mob = mobility(WHITE)+mobility(BLACK);
    pawns = pawnsetup(WHITE)-pawnsetup(BLACK);

    res = mat * 5 + pawns * 2 + mob;
    return res;
}

int evalmove(move m, int color)
{
    movelist *undo;
    struct variables v;
    int res;
    
    undo = domove(m, &v);
    res = evalboard(color);
    undomove(&undo);
    restorevariables(v);
    return res;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
char *algebraic(move m)
{
    char *res = malloc(7*sizeof(char));

    strcpy(res, "a0-a0  ");
    
    res[0]+=getx(gets1(m));
    res[1]+=gety(gets1(m))+1;
    res[3]+=getx(gets2(m));
    res[4]+=gety(gets2(m))+1;
    if (m&(1<<31))
	res[5]='+';
    if ((m&(3<<30))==(3<<30))
	res[6]='+';
    return res;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* alpha beta search #1 */
int findbest(int color, int depth, int max,
	     int alpha, int beta)
{
    int adv, reply;
    movelist *list, *undo;
    move m;

    struct variables v;
    int newmax=max;
    int count = 0;
    
    adv = evalboard(color);
    
    if (depth == 0)
	return adv;

    list = validmoves(color);

    if (validmoves==NULL)
    {
	return adv;
    }
    
    if (color==WHITE)
    {
	if (adv >= STALEMATE)
	    return adv;

	adv = alpha;
    }
    else
    {
	if (adv <= -STALEMATE)
	    return adv;

	adv = beta;
    }
    
    while (list)
    {
	count ++;
	/*
	if (count > max)
	
	*/
	m = pop (&list);
	
	undo = domove(m, &v);
	reply = findbest(-color, depth-1, newmax, alpha, beta);
	undomove(&undo);
	restorevariables(v);
	
	if ( ((color == WHITE) && (reply > adv))||
	     ((color == BLACK) && (reply < adv)) )
	{
	    if (color==WHITE)
		alpha = adv = reply;
	    else
		beta = adv = reply;

	    bestarray[depth]=m;
	    
	    if (alpha >= beta)
	    {
		/*
		printf(".");
		fflush(stdout);
		*/
		deletelist(list);
		return adv;
 	    }
	}
    }
    return adv;
}

/* alpha beta search #1 */
int findbest2(int color, int depth,
	      int alpha, int beta)
{
    int adv, reply;
    movelist *list, *undo;
    move m;
    struct variables v;
    
    adv = evalboard2(color);
    
    if (depth == 0)
	return adv;

    list = validmoves(color);

    if (validmoves==NULL)
	return adv;
    
    if (color==WHITE)
    {
	if (adv >= STALEMATE)
	{
	    deletelist(list);
	    return adv;
	}
	
	adv = alpha;
    }
    else
    {
	if (adv <= -STALEMATE)
	{
	    deletelist(list);
	    return adv;
	}
	
	adv = beta;
    }
    

    while (list)
    {
	m = pop(&list);

	undo = domove(m, &v);
	reply = findbest2(-color, depth-1, alpha, beta);
	undomove(&undo);
	restorevariables(v);

	if ( ((color == WHITE) && (reply > adv))||
	     ((color == BLACK) && (reply < adv)) )
	{
	    if (color==WHITE)
		alpha = adv = reply;
	    else
		beta = adv = reply;

	    bestarray[depth]=m;

	    if (alpha >= beta)
	    {
		/*
		printf(".");
		fflush(stdout); 
		*/
		deletelist(list);
		return adv;
	    }
	}
    }
    return adv;
}


/* alpha beta search #2 */
int findbest3(int color, int depth, int use, int pass)
{
    movelist *list;

    int eval = evalboard2 (color);
    int neweval;
    movelist *undo;
    move m;
    struct variables v;

    if (depth == 0)
	return eval;
    
    list = validmoves(color);
    
    if (list == NULL)
    {
	if (incheckp(color))
	    return -CHECKMATE;

	else if (eval < 0)
	    return STALEMATE;
	
	else return eval;
    }

    while (list)
    {
	m = pop(&list);
	
	undo=domove(m, &v);
	neweval = -findbest3(-color,depth-1,-pass,-use);
	undomove(&undo);
	restorevariables(v);

	if (neweval>pass)
	{
	    pass = neweval;
	    bestarray[depth]=m;
	}
	if (pass>=use)
	{
	    bestarray[depth]=m;
	    deletelist(list);
	    return pass;
	}
    }

    return pass;
}


move player1(int color)
{
    move m=NILMOVE;
    int savedplayer = computercontrolled[1];
    int res;
    
    
    computercontrolled[1]=1;
    
    res = findbest(color, movedepth[0], 30, -CHECKMATE, CHECKMATE);

    computercontrolled[1]=savedplayer;

    printsearchresults(res, color);
    
    m = bestarray[movedepth[0]];
    return m;
}


move player3(int color)
{
    move m = NILMOVE;
    int savedplayer = computercontrolled[0];
    int res;
    
    computercontrolled[0]=1;
    
    res = findbest3(color, movedepth[1], CHECKMATE, -CHECKMATE);

    computercontrolled[0]=savedplayer;

    printsearchresults(res, color);

    m = bestarray[movedepth[1]];
    return m;
}

move player2(int color)
{
    move m = NILMOVE;
    int savedplayer = computercontrolled[0];
    int res;
    
    computercontrolled[0]=1;

    res = findbest2(color, movedepth[1], -CHECKMATE, CHECKMATE);
    
    computercontrolled[0]=savedplayer;

    printsearchresults(res, color);
    m = bestarray[movedepth[1]];
    return m;
}

move compmove(int color)
{
    /*int r;*/
    move res;

    printf("thinking...");
    fflush(stdout);

    if (color==WHITE)
	res = player1(color);
    else
	res = player2(color);

    /*
    r = random() % 3;
    switch(r)
    {
	case 0:
	    res = player1(color);
	    break;
	case 1:
	    res = player2(color);
	    break;
	case 2:
	    res = player3(color);
	    break;
	default:
	    res = NILMOVE;
    }
    */
    return res;
}

int playchess(int color)
{
    static int wc = 0;
    static int bc = 0;
    
    move m;
    int done = 0;
    struct variables vars;
    movelist *undo;
    int valid;
    
    if (color==WHITE) wc++;
    else bc++;
    
    globals.turn = color;

    printf("\n- valid moves -\n");
    undo = validmoves(color);
    printalist(undo);
    deletelist(undo);
    printf("---------------\n");
    
    do 
    {
	printboard(color);

	if (gameoverp())
	    return 1;

	if (color == WHITE)
	{
	    printf("%d W : ", wc);
	    if (computercontrolled[0])
		m = compmove(color);
	    else
		m = getmove();
	}
	else
	{
	    printf("%d B : ", bc);
	    if (computercontrolled[1])
		m = compmove(color);
	    else
		m = getmove();
	}

	
	printf("move %s", algebraic(m));
	printmove(m);

	if (m == NILMOVE)
	{
	    valid = gameoverp();
	    return 1;
	}
	
	
	if (m == DRAWREP)
	{
	    printf("draw request denied.  play on\n");
	    m = NILMOVE;
	}
	    
	valid = validp (m);
	if (valid)
	{
	    undo = domove(m, &vars);
	    done = 1;
	}
	else
	{
	    printf("invalid move\n");
	}

    } while (!done);

    deletelist(undo);
    
    if (color==WHITE)
    {
	if (incheckp(BLACK))
	    setcheck(&m);

	if (checkmatep(BLACK))
	    setmate(&m);
	push(&moves[0],m);
    }
    
    else
    {
	if (incheckp(WHITE))
	    setcheck(&m);
	if (checkmatep(WHITE))
	    setmate(&m);
	push(&moves[1],m);
    }
    
    return 0;
}


int main(int argc, char **argv)
{
    int done = 0;
    move m;
    int c;
    
    initialize();

    
    if (argc != 3)
    {
	printf("usage: %s whitedepth blackdepth\n", argv[0]);
	printf("depth of 0 means human player\n");
	return 255;
    }

    c = atoi(argv[1]);
    if (c>0)
    {
	computercontrolled[0]=1;
	movedepth[0]=c;
    }

    c = atoi(argv[2]);
    if (c>0)
    {
	computercontrolled[1]=1;
	movedepth[1]=c;
    }
     
    while (!done)
    {
	done = playchess(WHITE);

	if (done) break;

	done = playchess(BLACK);

    }
    printf("moves done = %d\n", movecount);

    c=0;

    moves[0] = reverse(moves[0]);
    moves[1] = reverse(moves[1]);
    
    printf("----\n");
    while(moves[0])
    {
	c++;
	m=pop(&moves[0]);
	printf("%2d W : %s\t",c,algebraic(m));
	
	if (moves[1])
	{
	    m=pop(&moves[1]);
	    printf("%2d B : %s",c, algebraic(m));
	}
	printf("\n");
    }
    printf("----\n");

    printf("List Pointers In Use: %d\n", meminuse());
    
    return 0;
}

piece test1(int x, int y)
{
    return getxy(x,x);		/* 10 instr */
}

piece b[12*12];

piece test2(int x, int y)
{
    return b[x*12+y+26];	/* 10 instr */
}

piece test3(int x, int y)
{
    return b[8*x+y];		/* 7  */
}
