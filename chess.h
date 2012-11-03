typedef enum { empty,wp,wn,wb,wr,wq,wk,bp,bn,bb,br,bq,bk } piece;
int piecevalues[] = { 0, 1, 3, 3, 5, 10, 200, -1, -3, -3, -5, -10, -200 };

typedef piece chessboard[8][8];

typedef unsigned int square;
typedef unsigned long int move;

struct Movelist
{
	move data;
	struct Movelist *next;
};

typedef struct Movelist movelist;

/*

typedef int variables;

bit 0 white o-o
bit 1 white o-o-o
bit 2 black o-o
bit 3 black o-o-o

bit 4-7  wenpassant (0-15)
bit 8-11 benpassant (0-15)
bit 12   turn (0=black, 1=white)

#define whitecanoo(v)  (v&(1<<0))
#define whitecanooo(v) (v&(1<<1))
#define blackcanoo(v)  (v&(1<<2))
#define blackcanooo(v) (v&(1<<3))
#define wenpassant(v)  (v>>4&(0xf))
#define benpassant(v)  (v>>8&(0xf))
#define getturn(v)     ((v&(1<<12)) ? 1 : -1)
*/


struct variables 
{
	int wcanoo:1;
	int wcanooo:1;
	int bcanoo:1;
	int bcanooo:1;
	int wenpassant:4;
	int benpassant:4;
	signed int turn:2;
};

movelist *nmoves(square loc);
movelist *bmoves(square loc);
movelist *rmoves(square loc);
movelist *qmoves(square loc);
movelist *kmoves(square loc);
int pmovecount(square loc);
int nmovecount(square loc);
int bmovecount(square loc);
int rmovecount(square loc);
int qmovecount(square loc);
int kmovecount(square loc);
int pawnsetup(int color);
int material();
int mobility(int color);
movelist *validmoves(int color);
int checkchecker (move m, int color);
square findking(int color);
movelist *locations(int color);
int incheckp (int color);
int validp(move m);
int validp_nocheck(move m);
int isvalid(move m,int check);
piece choosepiece(int color);
void printvariables (struct variables v);
void restorevariables(struct variables v);
movelist *domove(move m, struct variables *oldv);
void undomove(movelist **list);
int  evalboard(int color);
int  evalboard2(int color);
int  evalmove(move m, int color);
char *algebraic(move m);
int  findbest(int color, int depth, int max,int alpha, int beta);
int  findbest2(int color, int depth, int alpha, int beta);
move player1(int color);
move player2(int color);
move compmove(int color);
int  playchess(int color);
int  main(int argc, char **argv);
/*
void testlist();
int timestuff();
*/
