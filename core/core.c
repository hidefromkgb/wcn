#include "ogl_load/ogl_load.h"
#include "vec_math/vec_math.h"
#include "core.h"

#include <unistd.h>
#include <fcntl.h>



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



char *shdr[] = {
/** === main vertex shader **/
"uniform mat4 mMVP;"
"attribute vec3 vert;"
"varying vec3 vpos;"
"void main() {"
    "vpos = vert;"
    "gl_Position = mMVP * vec4(vert, 1.0);"
"}",
/** === main pixel shader **/
"uniform sampler2D tile;"
"varying vec3 vpos;"
"void main() {"
    "vec3 ctex;"
    "if (abs(vpos.x) > 0.999)"
        "ctex = vpos.yzx;"
    "else if (abs(vpos.y) > 0.999)"
        "ctex = vpos.zxy;"
    "else if (abs(vpos.z) > 0.999)"
        "ctex = vpos.xyz;"
    "gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);"
"}"};



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

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
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

VEC_T3FV ReadI32T3F(char *pos) {
    return (VEC_T3FV){{ 1024.f / 0x7FFFFFFF * I32_SWAP(((int32_t*)pos)[0]),
                       -1024.f / 0x7FFFFFFF * I32_SWAP(((int32_t*)pos)[1]),
                       -1024.f / 0x7FFFFFFF * I32_SWAP(((int32_t*)pos)[2])}};
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

void PutTag(long bgn, long end, uint32_t fclr, uint32_t nclr, char *name) {
    static long id = 0;
    printf("    <TAG id=\"%ld\">\n      <start_offset>%ld</start_offset>\n      <end_offset>%ld</end_offset>\n"
           "      <tag_text>%s</tag_text>\n      <font_colour>#%06X</font_colour>\n"
           "      <note_colour>#%06X</note_colour>\n    </TAG>\n", id++, bgn, end, name, fclr, nclr);
}

GLenum ImportWL3(OGL_UNIF *pind, OGL_UNIF *pver, char *name) {
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

// "ebike/ebike.wl3"        // player
// "fjende1/fjende1.wl3"    // enemy #1
// "fjende2/fjende2.wl3"    // enemy #2
// "fjende3/fjende3.wl3"    // enemy #3
// "fjende4/fjende4.wl3"    // enemy #4
// "fjende5/fjende5.wl3"    // enemy #5
// "pokal/pokal.wl3"        // championship cup
// "ting/ting.wl3"          // bike material (glow/flash)

// "pickbost/pickbost.wl3"  // booster
// "pickjmp/pickjmp.wl3"    // jumper

// "camera/camera.wl3"      // EMPTY (???)
// "wirecam/wirecam.wl3"    // some camera asset, never seen in-game

    #pragma pack(push, 1)
    struct WL3H {           // Main Header
        uint32_t offsPart;  // offset of the Part Table
        uint32_t offsUnk;   // offset to some block of unknown purpose, commonly close to the vertex data
        uint32_t null1;     // usually equals 0
        uint32_t offsTail;  // offset to some array at the very end of the file

        uint32_t numPart;   // size of the Part Table

        uint32_t numVert;   // total vertex count
        uint32_t numPrim;   // total primitive count

        uint32_t unk1[9];   // UNKNOWN
        uint16_t unk2[6];   // UNKNOWN; tightly packed, may happen to be uint8_t`s

        uint8_t name[16];   // name of the model
        uint16_t hdrSize;   // remaining header size
        uint16_t hdrObjs;   // remaining header object count
    } *wl3h;

    struct PART {           // Part Table
        uint32_t qidx;      // offset: prim indices
        uint32_t vert;      // offset: vertices
        uint32_t texc;      // offset: texture coords
        uint32_t qnrm;      // offset: per-prim normals
        uint32_t vnrn;      // offset: per-vertex normals
        uint32_t attr;      // offset: prim attributes

        uint32_t numVert;   // vertex count for this part
        uint32_t numPrim;   // primitive count for this part

        uint32_t unk1[3];   // UNKNOWN

        uint32_t offsX;     // offset of the part in the model: X
        uint32_t offsY;     // offset of the part in the model: Y
        uint32_t offsZ;     // offset of the part in the model: Z

        uint32_t unk2[8];   // UNKNOWN

        uint8_t tex[12];    // texture ID

        uint16_t partSize;  // remaining part size
        uint16_t partObjs;  // remaining part object count
    } *part;
    #pragma pack(pop)

    char *fptr, *file = rLoadFile(name, 0);
    long cind = 0, cver = 0;

    if (!file) {
        printf("'%s': cannot load the file! Exiting.\n", name);
        exit(2);
    }

    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<wxHexEditor_XML_TAG>\n  <filename path=\"%s\">\n", name);

    wl3h = (void*)file;

    pind->type = 0;
    pind->pdat = calloc(1, pind->cdat = U32_SWAP(wl3h->numPrim) * 4 * sizeof(GLuint));
    pver->type = OGL_UNI_T3FV;
    pver->pdat = calloc(1, pver->cdat = U32_SWAP(wl3h->numVert) * sizeof(VEC_T3FV));

    PutTag(0, 3, 0, 0x55C6C3, "");
    PutTag((char*)&wl3h->numVert - file, (char*)&wl3h->numVert - file + 3, 0, 0xCE5C00, "Total vertex count");
    PutTag((char*)&wl3h->numPrim - file, (char*)&wl3h->numPrim - file + 3, 0, 0xCE5C00, "Total prim count");

    // at the Part Table offset there`s a vector prepended by its total byte size that contains offsets to Part Tables
    // (except the last one - it comes directly after the said vector)
    for (long offs = U32_SWAP(wl3h->offsPart), size = U32_SWAP(*(uint32_t*)(file + offs)), iter = 4;
         iter <= size; iter += 4) {
        if (iter == 4)
            PutTag(offs, offs + size - 1, 0, 0x55C6C3, "");
        part = (void*)(file + offs + ((iter == size) ? size : U32_SWAP(*(uint32_t*)(file + offs + iter))));

        PutTag((char*)part - file +   0, (char*)part - file +   3, 0, 0x9070E0, "Part: prim indices");
        PutTag((char*)part - file +   4, (char*)part - file +   7, 0, 0x9070E0, "Part: vertices");
        PutTag((char*)part - file +   8, (char*)part - file +  11, 0, 0x9070E0, "Part: texcoords");
        PutTag((char*)part - file +  12, (char*)part - file +  15, 0, 0x9070E0, "Part: prim normals");
        PutTag((char*)part - file +  16, (char*)part - file +  19, 0, 0x9070E0, "Part: vertex normals");
        PutTag((char*)part - file +  20, (char*)part - file +  23, 0, 0x9070E0, "Part: prim attrs");
        PutTag((char*)part - file +  24, (char*)part - file +  27, 0, 0x9070E0, "Part: vertex count");
        PutTag((char*)part - file +  28, (char*)part - file +  31, 0, 0x9070E0, "Part: prim count");
        PutTag((char*)part - file +  88, (char*)part - file +  99, 0, 0x9070E0, "Part: texture");
        PutTag((char*)part - file + 100, (char*)part - file + 101, 0, 0x901090, "Part: tail size");
        PutTag((char*)part - file + 102, (char*)part - file + 103, 0, 0x901090, "Part: tail objs");

        fptr = (char*)part + U32_SWAP(part->qidx) + 2 * sizeof(uint16_t);  // also skipping two counters

        PutTag(fptr - file - 4, fptr - file - 3, 0, 0xCF5C00, "triangle count");
        PutTag(fptr - file - 2, fptr - file - 1, 0, 0xCF5C00, "quad count (\?\?\?)");

        long tri = U16_SWAP(((uint16_t*)fptr)[-2]), qua = U32_SWAP(part->numPrim) - tri;
        for (long iter = 0; iter < tri; iter++) {
            ((GLuint*)pind->pdat)[cind + iter * 4 + 0] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 0]) >> 1);
            ((GLuint*)pind->pdat)[cind + iter * 4 + 1] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 1]) >> 1);
            ((GLuint*)pind->pdat)[cind + iter * 4 + 2] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 2]) >> 1);
            ((GLuint*)pind->pdat)[cind + iter * 4 + 3] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 3 + 2]) >> 1);
            PutTag(fptr - file + iter * 6, fptr - file + iter * 6 + 5, 0, 0xCF5C00, "");
        }
        cind += tri * 4;
        for (long iter = 0; iter < qua; iter++) {
            ((GLuint*)pind->pdat)[cind + iter * 4 + 0] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 0]) >> 1);
            ((GLuint*)pind->pdat)[cind + iter * 4 + 1] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 1]) >> 1);
            ((GLuint*)pind->pdat)[cind + iter * 4 + 2] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 2]) >> 1);
            ((GLuint*)pind->pdat)[cind + iter * 4 + 3] = cver + (U16_SWAP(((uint16_t*)fptr)[iter * 4 + 3]) >> 1);
            PutTag(fptr - file + tri * 6 + iter * 8, fptr - file + tri * 6 + iter * 8 + 7, 0, 0xCF5C00, "");
        }
        cind += qua * 4;

        fptr = (char*)part + U32_SWAP(part->vert);
        VEC_T3FV plus = ReadI32T3F((char*)&part->offsX);
        for (long iter = 0, size = U32_SWAP(part->numVert); iter < size; iter++) {
            ((VEC_T3FV*)pver->pdat)[cver + iter] = ReadI16T3F(fptr + iter * 3 * sizeof(uint16_t));
            VEC_V3AddV(&((VEC_T3FV*)pver->pdat)[cver + iter], &plus);
            PutTag(fptr - file + iter * 6, fptr - file + (iter + 1) * 6 - 1, 0, 0x729FCF, "");
        }
        cver += U32_SWAP(part->numVert);
    }

    printf("  </filename>\n</wxHexEditor_XML_TAG>\n");

    free(file);
    return GL_QUADS;
}



ENGC *cMakeEngine(char *name) {
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

    OGL_UNIF attr[] =
        {{/** indices **/ .draw = GL_STATIC_DRAW},
         {.name = "vert", .draw = GL_STATIC_DRAW}};

    OGL_UNIF puni[] =
        {{.name = "mMVP", .type = OGL_UNI_TMFV, .pdat = &retn->view},
         {.name = "tile", .type = OGL_UNI_T1II, .pdat = (GLvoid*)0}};

    GLenum prim = ImportWL3(&attr[0], &attr[1], name);
    retn->fvbo = OGL_MakeVBO(1, prim, sizeof(attr) / sizeof(*attr), attr,
                                      sizeof(puni) / sizeof(*puni), puni,
                                      sizeof(shdr) / sizeof(*shdr), shdr);
    free(attr[0].pdat);
    free(attr[1].pdat);
    *OGL_BindTex(retn->fvbo, 0, OGL_TEX_NSET) = MakeTileTex(9, 5, 1);
    return retn;
}



void cFreeEngine(ENGC **engc) {
    OGL_FreeVBO(&(*engc)->fvbo);

    VEC_PurgeMatrixStack(&(*engc)->proj);
    VEC_PurgeMatrixStack(&(*engc)->view);

    free(*engc);
    *engc = 0;
}
