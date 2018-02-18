/*
 * parse.h - this file contains defines for model file reading.
 *
 *  $Id: parse.h,v 1.23 2004/02/03 05:48:12 johns Exp $
 */

#define PARSENOERR       0
#define PARSEBADFILE     1
#define PARSEBADSUBFILE  2
#define PARSEBADSYNTAX   4
#define PARSEEOF         8
#define PARSEALLOCERR    16
 
unsigned int readmodel(const char *, SceneHandle);

#ifdef PARSE_INTERNAL
#define TEXNAMELEN 255

typedef struct {
        char * name;
        void * tex;
} texentry;

typedef struct {
  FILE * ifp;            /* current input file pointer */
  const char * filename; /* filename of current input file */
  texentry *textable;    /* texture lookup table */
  texentry defaulttex;   /* The default texture when a lookup fails */
  int numtextures;       /* number of TEXDEF textures               */
  int maxtextures;       /* number of TEXDEF textures               */
  int numobjectsparsed;  /* total number of objects parsed so far   */
  rt_hash_t texhash;     /* hash table for texture name lookup      */
} parsehandle;  

typedef struct {
   double rx1; double rx2; double rx3;
   double ry1; double ry2; double ry3;
   double rz1; double rz2; double rz3;
} RotMat;

typedef unsigned int errcode;

/* texture lookup table code */
static void reset_tex_table(parsehandle *, SceneHandle);
static void free_tex_table(parsehandle *, SceneHandle);
static errcode add_texture(parsehandle *, void * tex, const char *);
static void * find_texture(parsehandle *, const char *);

/* scene file parsing code */
static errcode GetString(parsehandle *, const char *);
static errcode GetScenedefs(parsehandle *, SceneHandle);
static errcode GetCamera(parsehandle *, SceneHandle);
static errcode GetColor(parsehandle *, apicolor *);
static errcode GetFloat(parsehandle *, apiflt *);
static errcode GetVector(parsehandle *, apivector *);
static errcode GetTexDef(parsehandle *, SceneHandle);
static errcode GetTexAlias(parsehandle *);
static errcode GetTexture(parsehandle *, SceneHandle, void **);
void * GetTexBody(parsehandle *, SceneHandle, int);
static errcode GetBackGnd(parsehandle *, SceneHandle);
static errcode GetFog(parsehandle *, SceneHandle);
static errcode GetCylinder(parsehandle *, SceneHandle);
static errcode GetFCylinder(parsehandle *, SceneHandle);
static errcode GetPolyCylinder(parsehandle *, SceneHandle);
static errcode GetSphere(parsehandle *, SceneHandle);
static errcode GetPlane(parsehandle *, SceneHandle);
static errcode GetRing(parsehandle *, SceneHandle);
static errcode GetBox(parsehandle *, SceneHandle);
static errcode GetVol(parsehandle *, SceneHandle);
static errcode GetTri(parsehandle *, SceneHandle);
static errcode GetSTri(parsehandle *, SceneHandle);
static errcode GetVCSTri(parsehandle *, SceneHandle);
static errcode GetDirLight(parsehandle *, SceneHandle);
static errcode GetLight(parsehandle *, SceneHandle);
static errcode GetSpotLight(parsehandle *, SceneHandle);
static errcode GetLandScape(parsehandle *, SceneHandle);
static errcode GetTPolyFile(parsehandle *, SceneHandle);
static errcode GetObject(parsehandle *, SceneHandle);
static errcode ReadIncludeFile(parsehandle *, const char *, SceneHandle);

#ifdef USELIBMGF
static errcode GetMGFFile(parsehandle *, SceneHandle);
#endif

#endif

