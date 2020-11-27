#include "ogl_load/ogl_load.h"
#include "vec_math/vec_math.h"
#include "core.h"

#include <unistd.h>
#include <fcntl.h>



#define STRINGIFY(D) STRINGIFY_(D)
#define STRINGIFY_(D) #D

#define DEF_FANG  0.01  /** Default angular step             **/
#define DEF_FTRN  0.05  /** Default translational step       **/

#define DEF_FFOV 45.0   /** Default perspective view field   **/
#define DEF_ZNEA  0.1   /** Default near clipping plane      **/
#define DEF_ZFAR 90.0   /** Default far clipping plane       **/



struct ENGC {
    VEC_FMST *view, *proj;

    OGL_FVBO *fvbo;

    VEC_T2IV angp;
    VEC_T2FV fang;
    VEC_T3FV ftrn;

    GLboolean keys[KEY_ALL_KEYS];
};



OGL_FTEX *MakeTileTex(GLuint size, GLuint tile, GLuint tbdr) {
    uint32_t tclr, *bptr;
    GLint x, y, u, v;
    OGL_FTEX *retn;

    size = pow(2.0, size);
    tile = pow(2.0, tile);
    bptr = calloc(size * size, sizeof(*bptr));
    for (y = size / tile; y > 0; y--)
        for (x = size / tile; x > 0; x--) {
            tclr = ((rand() % 32) + 192) * 0x010101;

            for (v = (y - 1) * tile; v < y * tile; v++)
                for (u = x * tile - tbdr; u < x * tile; u++)
                    bptr[size * v + u] = tclr - 0x505050;
            for (v = y * tile - tbdr; v < y * tile; v++)
                for (u = (x - 1) * tile; u < x * tile - tbdr; u++)
                    bptr[size * v + u] = tclr - 0x505050;

            for (v = (y - 1) * tile + tbdr; v < y * tile - tbdr; v++)
                for (u = (x - 1) * tile; u < (x - 1) * tile + tbdr; u++)
                    bptr[size * v + u] = tclr | 0x202020;
            for (v = (y - 1) * tile; v < (y - 1) * tile + tbdr; v++)
                for (u = (x - 1) * tile; u < x * tile - tbdr; u++)
                    bptr[size * v + u] = tclr | 0x202020;

            for (v = (y - 1) * tile + tbdr; v < y * tile - tbdr; v++)
                for (u = (x - 1) * tile + tbdr; u < x * tile - tbdr; u++)
                    bptr[size * v + u] = tclr;
        }
    retn = OGL_MakeTex(size, size, 0, GL_TEXTURE_2D,
                       GL_REPEAT, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR,
                       GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA, bptr);
    free(bptr);
    return retn;
}



void cUpdateState(ENGC *engc) {
    VEC_T3FV vadd;
    VEC_T2FV fang;

    if (engc->keys[KEY_W] ^ engc->keys[KEY_S]) {
        fang = (VEC_T2FV){{engc->fang.x + 0.5 * M_PI, engc->fang.y}};
        VEC_V3FromAng(&vadd, &fang);
        VEC_V3MulC(&vadd, (engc->keys[KEY_W])? DEF_FTRN : -DEF_FTRN);
        VEC_V3AddV(&engc->ftrn, &vadd);
    }
    if (engc->keys[KEY_A] ^ engc->keys[KEY_D]) {
        fang = (VEC_T2FV){{engc->fang.x, 0.0}};
        VEC_V3FromAng(&vadd, &fang);
        VEC_V3MulC(&vadd, (engc->keys[KEY_A])? DEF_FTRN : -DEF_FTRN);
        VEC_V3AddV(&engc->ftrn, &vadd);
    }
}



void cMouseInput(ENGC *engc, long xpos, long ypos, long btns) {
    if (~btns & 2)
        return;
    if (btns & 1) { /** 1 for moving state, 2 for LMB **/
        engc->fang.y += DEF_FANG * (GLfloat)(ypos - engc->angp.y);
        if (engc->fang.y < -M_PI) engc->fang.y += 2.0 * M_PI;
        else if (engc->fang.y > M_PI) engc->fang.y -= 2.0 * M_PI;

        engc->fang.x += DEF_FANG * (GLfloat)(xpos - engc->angp.x);
        if (engc->fang.x < -M_PI) engc->fang.x += 2.0 * M_PI;
        else if (engc->fang.x > M_PI) engc->fang.x -= 2.0 * M_PI;
    }
    engc->angp = (VEC_T2IV){{xpos, ypos}};
}



void cKbdInput(ENGC *engc, uint8_t code, long down) {
    engc->keys[code] = down;
}



void cResizeWindow(ENGC *engc, long xdim, long ydim) {
    GLfloat maty = DEF_ZNEA * tanf(0.5 * DEF_FFOV * VEC_DTOR),
            matx = maty * (GLfloat)xdim / (GLfloat)ydim;

    VEC_PurgeMatrixStack(&engc->proj);
    VEC_PushMatrix(&engc->proj);
    VEC_M4Frustum(engc->proj->curr,
                 -matx, matx, -maty, maty, DEF_ZNEA, DEF_ZFAR);
    VEC_PushMatrix(&engc->proj);

    VEC_PurgeMatrixStack(&engc->view);
    VEC_PushMatrix(&engc->view);
    VEC_PushMatrix(&engc->view);

    VEC_M4Multiply(engc->proj->prev->curr,
                   engc->view->prev->curr, engc->proj->curr);

    glViewport(0, 0, xdim, ydim);
}



void cRedrawWindow(ENGC *engc) {
    VEC_TMFV rmtx, tmtx, mmtx;

    if (!engc->proj)
        return;

    VEC_M4Translate(tmtx, engc->ftrn.x, engc->ftrn.y, engc->ftrn.z);
    VEC_M4RotOrts(rmtx, engc->fang.y, engc->fang.x, 0.0);
    VEC_M4Multiply(rmtx, tmtx, mmtx);
    VEC_M4Multiply(engc->proj->curr, mmtx, engc->view->curr);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    OGL_DrawVBO(engc->fvbo, 0, 0);
}



char *rLoadFile(char *name, long *size) {
    char *retn = 0;
    long file, flen;

    if ((file = open(name, O_RDONLY)) > 0) {
        flen = lseek(file, 0, SEEK_END);
        lseek(file, 0, SEEK_SET);
        retn = malloc(flen + 1);
        if (read(file, retn, flen) == flen) {
            retn[flen] = '\0';
            if (size)
                *size = flen;
        }
        else {
            free(retn);
            retn = 0;
        }
        close(file);
    }
    return retn;
}

#define I16_SWAP(v) ((int16_t)(((uint16_t)(v) >> 8) | ((uint16_t)(v) << 8)))
#define U16_SWAP(v) ((uint16_t)I16_SWAP(v))
#define I32_SWAP(v) ((int32_t)(U16_SWAP((uint32_t)(v) >> 16) | (U16_SWAP(v) << 16)))
#define U32_SWAP(v) ((uint32_t)I32_SWAP(v))

VEC_T3FV ReadI32T3F(VEC_T4IV *vec) {
    float scale = 0.5 / I32_SWAP(vec->x);
    return (VEC_T3FV){{ scale * I32_SWAP(vec->y),
                       -scale * I32_SWAP(vec->z),
                       -scale * I32_SWAP(vec->w)}};
}

VEC_T3FV ReadI16T3F(char *pos) {
    return (VEC_T3FV){{ 1.f / 0x7FFF * I16_SWAP(((int16_t*)pos)[0]),
                       -1.f / 0x7FFF * I16_SWAP(((int16_t*)pos)[1]),
                       -1.f / 0x7FFF * I16_SWAP(((int16_t*)pos)[2])}};
}

VEC_T3FV ReadI8T3F(char *pos) {
    return (VEC_T3FV){{ 1.f / 0x7F * ((int8_t*)pos)[0],
                       -1.f / 0x7F * ((int8_t*)pos)[1],
                       -1.f / 0x7F * ((int8_t*)pos)[2]}};
}

void PutTag(long bgn, long len, uint32_t fclr, uint32_t nclr, char *name) {
    static long id = 0;
    printf("    <TAG id=\"%ld\">\n      <start_offset>%ld</start_offset>\n      <end_offset>%ld</end_offset>\n"
           "      <tag_text>%s</tag_text>\n      <font_colour>#%06X</font_colour>\n"
           "      <note_colour>#%06X</note_colour>\n    </TAG>\n", id++, bgn, bgn + len - 1, name, fclr, nclr);
}

void ImportWL3(OGL_UNIF *uvbo, char *name, bool xmlOnly) {
// Common values for 'name':

// "r4back01/r4back01.wl3"  // level 1 (the dump), part 1
// "r4back02/r4back02.wl3"  // level 1 (the dump), part 2
// "r4pspd01/r4pspd01.wl3"  // level 1 (the dump), booster line 1
// "r4rest/r4rest.wl3"      // level 1 (the dump), EMPTY (???)

// "r2back01/r2back01.wl3"  // level 2 (the suburbs), part 1
// "r2back02/r2back02.wl3"  // level 2 (the suburbs), part 2
// "r2back03/r2back03.wl3"  // level 2 (the suburbs), part 3
// "r2back04/r2back04.wl3"  // level 2 (the suburbs), part 4
// "r2pspd01/r2pspd01.wl3"  // level 2 (the suburbs), booster line 1
// "r2pspd02/r2pspd02.wl3"  // level 2 (the suburbs), booster line 2
// "r2trai03/r2trai03.wl3"  // level 2 (the suburbs), train
// "r2rest/r2rest.wl3"      // level 2 (the suburbs), EMPTY (???)

// "r3back01/r3back01.wl3"  // level 3 (tunnels), part 1
// "r3back02/r3back02.wl3"  // level 3 (tunnels), part 2
// "r3back03/r3back03.wl3"  // level 3 (tunnels), part 3
// "r3back04/r3back04.wl3"  // level 3 (tunnels), part 4
// "r3pspd01/r3pspd01.wl3"  // level 3 (tunnels), booster line 1
// "r3pspd02/r3pspd02.wl3"  // level 3 (tunnels), booster line 2
// "r3pspd03/r3pspd03.wl3"  // level 3 (tunnels), booster line 3
// "r3rest/r3rest.wl3"      // level 3 (tunnels), EMPTY (???)

// "r1back01/r1back01.wl3"  // level 4 (radioactive waste), part 1
// "r1back02/r1back02.wl3"  // level 4 (radioactive waste), part 2
// "r1back03/r1back03.wl3"  // level 4 (radioactive waste), part 3
// "r1back04/r1back04.wl3"  // level 4 (radioactive waste), part 4
// "r1pspd01/r1pspd01.wl3"  // level 4 (radioactive waste), booster line 1
// "r1pspd02/r1pspd02.wl3"  // level 4 (radioactive waste), booster line 2
// "r1pstp01/r1pstp01.wl3"  // level 4 (radioactive waste), blocker 1
// "r1rest/r1rest.wl3"      // level 4 (radioactive waste), EMPTY (???)

// "r5bdem01/r5bdem01.wl3"  // level 5 (downtown), demo 1
// "r5back01/r5back01.wl3"  // level 5 (downtown), part 1
// "r5kran02/r5kran02.wl3"  // level 5 (downtown), crane
// "r5pspd01/r5pspd01.wl3"  // level 5 (downtown), booster line 1
// "r5pspd02/r5pspd02.wl3"  // level 5 (downtown), booster line 2
// "r5pspd03/r5pspd03.wl3"  // level 5 (downtown), booster line 3
// "r5pstp01/r5pstp01.wl3"  // level 5 (downtown), blocker 1
// "r5rest/r5rest.wl3"      // level 5 (downtown), EMPTY (???)

// "r6back01/r6back01.wl3"  // level 6 (the spiral), part 1
// "r6rest/r6rest.wl3"      // level 6 (the spiral), EMPTY (???)

// "eback01/eback01.wl3"    // level E (credits)

// "back/back.wl3"          // EMPTY (???)
// "hiscr/hiscr.wl3"        // EMPTY (???)
// "ilogoscr/ilogoscr.wl3"  // EMPTY (???)
// "instscr/instscr.wl3"    // EMPTY (???)
// "optscr/optscr.wl3"      // EMPTY (???)
// "titlescr/titlescr.wl3"  // EMPTY (???)

// "skiltA/skiltA.wl3"      // roadblock sign
// "skiltB/skiltB.wl3"      // roadblock sign
// "skiltC/skiltC.wl3"      // roadblock sign
// "skiltD/skiltD.wl3"      // roadblock sign
// "skiltE/skiltE.wl3"      // roadblock sign

// "barrel/barrel.wl3"      // barrel
// "bjarne01/bjarne01.wl3"  // lorry

// "ebike/ebike.wl3"        // bike hull
// "fjende1/fjende1.wl3"    // enemy #1
// "fjende2/fjende2.wl3"    // enemy #2
// "fjende3/fjende3.wl3"    // enemy #3
// "fjende4/fjende4.wl3"    // enemy #4
// "fjende5/fjende5.wl3"    // enemy #5
// "pokal/pokal.wl3"        // championship cup
// "ting/ting.wl3"          // player

// "pickbost/pickbost.wl3"  // booster
// "pickjmp/pickjmp.wl3"    // jumper

// "camera/camera.wl3"      // EMPTY (???)
// "wirecam/wirecam.wl3"    // some camera asset, never seen in-game

    #pragma pack(push, 1)
    struct WL3H {           // Main Header
        uint32_t offsPart;  // offset of the Part Table
        uint32_t offsUnk;   // offset to some (W,W,D,D,D,D) block of unknown purpose
        uint32_t offsColi;  // the game refers to this value as 'offset to colitab', purpose unknown
        uint32_t offsTail;  // offset to some array at the very end of the file

        uint32_t numPart;   // size of the Part Table

        uint32_t numVert;   // total vertex count
        uint32_t numPrim;   // total primitive count

        uint32_t unk1[9];   // UNKNOWN
        uint32_t unk2[3];   // UNKNOWN

        uint8_t name[16];   // name of the model
        uint16_t hdrSize;   // remaining header size
        uint16_t hdrObjs;   // remaining header object count
    } *wl3h;
    struct PART {           // Part Table
        uint32_t pidx;      // offset: prim indices
        uint32_t vert;      // offset: vertices
        uint32_t texc;      // offset: texture coords
        uint32_t pnrm;      // offset: per-prim normals
        uint32_t vnrm;      // offset: per-vertex normals
        uint32_t attr;      // offset: prim attributes

        uint32_t numVert;   // vertex count for this part
        uint32_t numPrim;   // primitive count for this part

        uint32_t unk1[2];   // UNKNOWN

        VEC_T4IV offset;    // offsets: [scale, X, Y, Z]

        uint32_t unk2[3];   // UNKNOWN, probably a vector
        uint32_t unk3;      // UNKNOWN

        uint16_t always0C;  // theoretically should be the size of unk6, but seems to be fixed
        uint16_t unk5;      // UNKNOWN
        uint32_t unk6[3];   // UNKNOWN, [2] gets initialized with '-2'

        uint8_t tex[12];    // texture ID

        uint16_t partSize;  // remaining part size
        uint16_t partObjs;  // remaining part object count
    } *part;
    #pragma pack(pop)

    char *fptr, *file = rLoadFile(name, 0);
    long cind = 0;

    if (!file) {
        printf("'%s': cannot load the file! Exiting.\n", name);
        exit(2);
    }

    if (xmlOnly)
        printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<wxHexEditor_XML_TAG>\n  <filename path=\"%s\">\n", name);

    wl3h = (void*)file;

    uvbo[0].type = 0;
    uvbo[1].type = OGL_UNI_T3FV;
    uvbo[2].type = OGL_UNI_T3FV;
    uvbo[3].type = OGL_UNI_T3FV;
    uvbo[0].pdat = calloc(1, uvbo[0].cdat = U32_SWAP(wl3h->numPrim) * 4 * sizeof(GLuint));
    uvbo[1].pdat = calloc(1, uvbo[1].cdat = U32_SWAP(wl3h->numPrim) * 4 * sizeof(VEC_T3FV));
    uvbo[2].pdat = calloc(1, uvbo[2].cdat = U32_SWAP(wl3h->numPrim) * 4 * sizeof(VEC_T3FV));
    uvbo[3].pdat = calloc(1, uvbo[3].cdat = U32_SWAP(wl3h->numPrim) * 4 * sizeof(VEC_T3FV));

    if (xmlOnly) {
        PutTag((char*)&wl3h->offsPart - file, 4, 0x000000, 0x55C6C3, "part table offset");
        PutTag((char*)&wl3h->offsUnk - file, 4, 0x000000, 0x906000, "unknown struct offset");
        PutTag((char*)&wl3h->offsColi - file, 4, 0x000000, 0xF03030, "colitab offset");
        PutTag((char*)&wl3h->offsTail - file, 4, 0x000000, 0x9A1490, "tail offset");
        PutTag((char*)&wl3h->numPart - file, 4, 0x000000, 0x55C6C3, "part table size");
        PutTag((char*)&wl3h->numVert - file, 4, 0x000000, 0x204A87, "total vertex count");
        PutTag((char*)&wl3h->numPrim - file, 4, 0x000000, 0xCF5C00, "total prim count");
        PutTag((char*)&wl3h->name - file, 16, 0x000000, 0x5ED505, "name");

        PutTag((char*)&wl3h->hdrSize - file, 2, 0x000000, 0xEFD43B, "header size");
        PutTag((char*)&wl3h->hdrSize - file + 2, 2, 0x000000, 0xEFD43B, "header objs");
        PutTag((char*)&wl3h->hdrSize - file + 4, U16_SWAP(wl3h->hdrSize), 0x000000, 0xEFD43B, "header");

        fptr = (char*)file + U32_SWAP(wl3h->offsUnk);
        for (long iter = 0, size = (fptr != file) ? U32_SWAP(wl3h->numPart) * 2 - 1 : 0; iter < size; iter++) {
            PutTag(fptr - file + iter * 20, 20, 0x000000, (iter & 1) ? 0x906000 : 0xF0C070, "");
        }
    }

    fptr = (char*)file + U32_SWAP(wl3h->offsColi);
    if ((fptr != file) && xmlOnly)
        PutTag(fptr - file, 2, 0x000000, 0xF03030, "colitab");

    fptr = (char*)file + U32_SWAP(wl3h->offsTail) + 2;
    if (xmlOnly) {
        PutTag(fptr - file - 2, 2, 0x000000, 0x9A1490, "tail size");
        PutTag(fptr - file, U16_SWAP(((uint16_t*)fptr)[-1]) * 2, 0x000000, 0x9A1490, "tail");
    }

    // at the Part Table offset there`s a vector prepended by its total byte size that contains offsets to Part Tables
    // (except the last one - it comes directly after the said vector)
    for (long offs = U32_SWAP(wl3h->offsPart), size = U32_SWAP(*(uint32_t*)(file + offs)), iter = 4;
         iter <= size; iter += 4) {
        if ((iter == 4) && xmlOnly)
            PutTag(offs, size, 0x000000, 0x55C6C3, "part table");
        part = (void*)(file + offs + ((iter == size) ? size : U32_SWAP(*(uint32_t*)(file + offs + iter))));

        if (xmlOnly) {
            PutTag((char*)part - file +   0, 4, 0x0000FF, 0x9070E0, "part: prim indices");
            PutTag((char*)part - file +   4, 4, 0x000000, 0x9070E0, "part: vertices");
            PutTag((char*)part - file +   8, 4, 0x000000, 0x9070E0, "part: texcoords");
            PutTag((char*)part - file +  12, 4, 0x000000, 0x9070E0, "part: prim normals");
            PutTag((char*)part - file +  16, 4, 0x000000, 0x9070E0, "part: vertex normals");
            PutTag((char*)part - file +  20, 4, 0x000000, 0x9070E0, "part: prim attrs");
            PutTag((char*)part - file +  24, 4, 0x000000, 0x204A87, "part: vertex count");
            PutTag((char*)part - file +  28, 4, 0x000000, 0xCF5C00, "part: prim count");
            PutTag((char*)part - file +  32, 4 * 3, 0x000000, 0x41F356, "part: (D,D, part scale)");
            PutTag((char*)part - file +  44, 4 * 3, 0x000000, 0x41F356, "part: offsets (X,Y,Z)");
            PutTag((char*)part - file +  56, 4 * 3, 0x000000, 0x41F356, "part: (D,D,D)");
            PutTag((char*)part - file +  68, 4, 0x000000, 0x557C2A, "part: (usually 0) (?)");
            PutTag((char*)part - file +  72, 16, 0x000000, 0x557C2A, "part: (W,W,D,D,D)");
            PutTag((char*)part - file +  88, 12, 0x000000, 0x9070E0, "part: texture");
            PutTag((char*)part - file + 100, 2, 0x000000, 0x901090, "part: tail size");
            PutTag((char*)part - file + 102, 2, 0x000000, 0x901090, "part: tail objs");
        }

        /// indices: triangles

        fptr = (char*)part + U32_SWAP(part->pidx) + 2;

        if (xmlOnly)
            PutTag(fptr - file - 2, 2, 0x0000FF, 0xFCAF3E, "triangle count");

        long tri = U16_SWAP(((uint16_t*)fptr)[-1]);
        for (long iter = 0; iter < tri; iter++) {
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 0] = (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 0]) >> 1);
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 1] = (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 1]) >> 1);
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 2] = (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 2]) >> 1);
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 3] = (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 2]) >> 1);
            if (xmlOnly)
                PutTag(fptr - file + iter * 6, 6, 0x000000, (iter & 1) ? 0xFCAF3E : 0xCF5C00, "");
        }
        cind += tri * 4;
        fptr += tri * 6 + 2;

        /// indices: quads

        if (xmlOnly)
            PutTag(fptr - file - 2, 2, 0x000000, (tri & 1) ? 0xFCAF3E : 0xCF5C00, "quad count");

        long qua = U16_SWAP(((uint16_t*)fptr)[-1]);
        for (long iter = 0; iter < qua; iter++) {
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 0] = (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 0]) >> 1);
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 1] = (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 1]) >> 1);
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 2] = (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 2]) >> 1);
            ((GLuint*)uvbo[0].pdat)[cind + iter * 4 + 3] = (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 3]) >> 1);
            if (xmlOnly)
                PutTag(fptr - file + iter * 8, 8, 0x000000, ((tri + iter) & 1) ? 0xCF5C00 : 0xFCAF3E, "");
        }
        cind += qua * 4;

        long vert = U32_SWAP(part->numVert), prim = U32_SWAP(part->numPrim);

        /// vertices

        fptr = (char*)part + U32_SWAP(part->vert);
        VEC_T3FV plus = ReadI32T3F(&part->offset);
        float scale = (float)I32_SWAP(part->offset.x) / 0x7FFFF;
        for (long iter = 0; iter < prim; iter++) {
            for (long indx = 0; indx < 4; indx++) {
                VEC_T3FV *base = ((VEC_T3FV*)uvbo[1].pdat) + cind - prim * 4 + iter * 4 + indx;
                *base = ReadI16T3F(fptr + ((GLuint*)uvbo[0].pdat)[cind - prim * 4 + iter * 4 + indx] * 3 * 2);
                VEC_V3AddV(base, &plus);
                VEC_V3MulC(base, scale);
            }
        }
        for (long iter = (xmlOnly) ? 0 : vert; iter < vert; iter++)
            PutTag(fptr - file + iter * 6, 6, (iter) ? 0x000000 : 0x0000FF, (iter & 1) ? 0x204A87 : 0x729FCF, "");

        /// texcoords

        fptr = (char*)part + U32_SWAP(part->texc) + 2;
        if (xmlOnly)
            PutTag(fptr - file - 2, 2, 0x0000FF, 0xFCAF3E, "texcoord count, never used");
        for (long iter = 0; iter < prim; iter++) {
            if (xmlOnly)
                PutTag(fptr - file + iter * 4, 4, 0x000000, (iter & 1) ? 0xFCAF3E : 0xCF5C00, "");
        }

        /// prim normals

        fptr = (char*)part + U32_SWAP(part->pnrm);
        for (long iter = 0; iter < prim; iter++) {
            VEC_T3FV *base = ((VEC_T3FV*)uvbo[2].pdat) + cind - prim * 4 + iter * 4;
            VEC_T3FV norm = ReadI8T3F(fptr + iter * 3);
            VEC_V3MulC(&norm, -1.f);
            VEC_V3Normalize(&norm);
            base[0] = base[1] = base[2] = base[3] = norm;
            if (xmlOnly)
                PutTag(fptr - file + iter * 3, 3, (iter) ? 0x000000 : 0x0000FF, (iter & 1) ? 0x204A87 : 0x729FCF, "");
        }

        /// vertex normals

        fptr = (char*)part + U32_SWAP(part->vnrm);
        for (long iter = 0; iter < tri; iter++) {
            if (xmlOnly) {
                PutTag(fptr - file + iter * 3 * 3 + 0, 3,
                      (iter * 3 + 0) ? 0x000000 : 0x0000FF, ((iter * 3 + 0) & 1) ? 0x204A87 : 0x729FCF, "");
                PutTag(fptr - file + iter * 3 * 3 + 3, 3,
                      (iter * 3 + 1) ? 0x000000 : 0x0000FF, ((iter * 3 + 1) & 1) ? 0x204A87 : 0x729FCF, "");
                PutTag(fptr - file + iter * 3 * 3 + 6, 3,
                      (iter * 3 + 2) ? 0x000000 : 0x0000FF, ((iter * 3 + 2) & 1) ? 0x204A87 : 0x729FCF, "");
            }
        }
        fptr += tri * 3 * 3;
        for (long iter = 0; iter < qua; iter++) {
            if (xmlOnly) {
                PutTag(fptr - file + iter * 4 * 3 + 0, 3, 0x000000, ((iter * 4 + 0) & 1) ? 0x204A87 : 0x729FCF, "");
                PutTag(fptr - file + iter * 4 * 3 + 3, 3, 0x000000, ((iter * 4 + 1) & 1) ? 0x204A87 : 0x729FCF, "");
                PutTag(fptr - file + iter * 4 * 3 + 6, 3, 0x000000, ((iter * 4 + 2) & 1) ? 0x204A87 : 0x729FCF, "");
                PutTag(fptr - file + iter * 4 * 3 + 9, 3, 0x000000, ((iter * 4 + 3) & 1) ? 0x204A87 : 0x729FCF, "");
            }
        }

        /// prim attributes

        fptr = (char*)part + U32_SWAP(part->attr);
        for (long iter = 0; iter < prim; iter++) {
            uint16_t clr = U16_SWAP(((uint16_t*)fptr)[iter]);
            ((VEC_T3FV*)uvbo[3].pdat)[((GLuint*)uvbo[0].pdat)[cind + (iter - prim) * 4 + 3]] = (VEC_T3FV){{
                1.f / 0x1F * ((clr >> 10) & 0x1F), 1.f / 0x1F * ((clr >> 5) & 0x1F), 1.f / 0x1F * ((clr >> 0) & 0x1F)
            }};
            if (xmlOnly)
                PutTag(fptr - file + iter * 2, 2, (iter) ? 0x000000 : 0x0000FF, (iter & 1) ? 0xFCAF3E : 0xCF5C00, "");
        }
    }

    for (long iter = 0; iter * sizeof(GLuint) < uvbo[0].cdat; iter++) {
        ((GLuint*)uvbo[0].pdat)[iter] = iter;
    }

    if (xmlOnly)
        printf("  </filename>\n</wxHexEditor_XML_TAG>\n");

    free(file);
}



ENGC *cMakeEngine(char *name, bool xmlOnly) {
    ENGC *retn;

    retn = calloc(1, sizeof(*retn));

    retn->ftrn.x =  3.4;
    retn->ftrn.y = -3.8;
    retn->ftrn.z = -6.0;

    retn->fang.x = 30.00 * VEC_DTOR;
    retn->fang.y = 30.00 * VEC_DTOR;

    glClearColor(0.0, 0.0, 0.0, 1.0);

//    glCullFace(GL_BACK);
//    glEnable(GL_CULL_FACE);

    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);

    OGL_UNIF uvbo[] =
        {{/** indices **/ .draw = GL_STATIC_DRAW},
         {.name = "vert", .draw = GL_STATIC_DRAW},
         {.name = "norm", .draw = GL_STATIC_DRAW},
         {.name = "clrs", .draw = GL_STATIC_DRAW}};

    OGL_UNIF puni[] =
        {{.name = "mMVP", .type = OGL_UNI_TMFV, .pdat = &retn->view},
         {.name = "ftrn", .type = OGL_UNI_T3FV, .pdat = &retn->ftrn}};

    ImportWL3(uvbo, name, xmlOnly);
    retn->fvbo = OGL_MakeVBO(0, GL_QUADS, sizeof(uvbo) / sizeof(*uvbo), uvbo, sizeof(puni) / sizeof(*puni), puni,
                             2, (char*[]){
                                /** === main vertex shader **/
                                "#version 150\n"

                                "uniform mat4 mMVP;"

                                /** attributes **/
                                "attribute vec3 vert;"
                                "attribute vec3 norm;"
                                "attribute vec3 clrs;"

                                "uniform vec3 ftrn;"

                                "smooth out vec3 v;"
                                "flat out vec3 n;"
                                "flat out vec3 c;"

                                "void main() {"
                                    "v = -ftrn - vert;"
                                    "n = norm;"
                                    "c = clrs;"
                                    "gl_Position = mMVP * vec4(vert, 1.0);"
                                "}",

                                /** === main pixel shader **/
                                "#version 150\n"

                                "uniform sampler2D tile;"

                                "smooth in vec3 v;"
                                "flat in vec3 n;"
                                "flat in vec3 c;"

                                "void main() {"
                                    "const float DEF_ZFAR = %s;"
                                    "const vec3 lightColor = vec3(1.0, 1.0, 1.0);"
                                    "const vec3 ambient = vec3(0.1, 0.1, 0.1);"
                                    "vec3 clr = vec3(1.0, 1.0, 1.0);"
                                    "float dist = 1.0 - min(dot(v, v), DEF_ZFAR * DEF_ZFAR) / DEF_ZFAR / DEF_ZFAR;"
                                    "vec3 diffuse = lightColor * clamp(dot(n, normalize(v)), 0.0, 1.0) * dist;"
                                    "gl_FragColor = clamp(vec4(clr.rgb * (diffuse + ambient), 1.0), 0.0, 1.0);"
                                "}"}, STRINGIFY(DEF_ZFAR));

    free(uvbo[0].pdat);
    free(uvbo[1].pdat);
    free(uvbo[2].pdat);
    free(uvbo[3].pdat);

    if (xmlOnly) {
        cFreeEngine(&retn);
        exit(0);
    }

    return retn;
}



void cFreeEngine(ENGC **engc) {
    OGL_FreeVBO(&(*engc)->fvbo);

    VEC_PurgeMatrixStack(&(*engc)->proj);
    VEC_PurgeMatrixStack(&(*engc)->view);

    free(*engc);
    *engc = 0;
}
