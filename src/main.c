/* 
 * mtlsort <z64.me>
 *
 * modifies Wavefront OBJ and MTL files in such a way that
 * the materials declared in the MTL using 'newmtl' commands
 * are guaranteed to match the order of the corresponding
 * 'usemtl' commands in the OBJ; this is achieved by way of
 * duplicating materials in the desired order
 *
 * this is the messiest code I've written in a while, but
 * don't worry: valgrind reports no errors
 *
 * TODO strip comments from files before processing them
 * (those are lines beginning with '#'; no problems have
 * been encountered yet, but do it sometime anyway!)
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* minimal file loader
 * returns 0 on failure
 * returns pointer to loaded file on success
 */
void *loadfile(const char *fn, size_t *sz)
{
	FILE *fp;
	void *dat;
	
	/* rudimentary error checking returns 0 on any error */
	if (
		!fn
		|| !sz
		|| !(fp = fopen(fn, "rb"))
		|| fseek(fp, 0, SEEK_END)
		|| !(*sz = ftell(fp))
		|| fseek(fp, 0, SEEK_SET)
		|| !(dat = malloc(*sz + 1))
		|| fread(dat, 1, *sz, fp) != *sz
		|| fclose(fp)
	)
		return 0;
	
	return dat;
}


/* loads file as a zero-terminated string
 * returns 0 on failure
 * returns pointer to loaded file on success
 */
char *loadfilestring(const char *fn)
{
	char *c;
	size_t sz;
	
	c = loadfile(fn, &sz);
	
	if (!c)
		return 0;
	
	c[sz] = '\0';
	
	return c;
}

/* given a pointer to a string in an ASCII text file,
 * returns the pointer to the next line, or 0 if no line follows
 */
char *nextline(char *s)
{
	char *sOld = s;
	
	if (!s || !*s)
		return 0;
	
	/* advance to next line, returning 0 if there isn't one */
	s += strcspn(s, "\r\n");
	if (s == sOld)
		return 0;
	while (*s && iscntrl(*s))
		++s;
	if (!*s)
		return 0;
	
	return s;
}

void mtlsort(char *obj, char *mtl, FILE *objOut, FILE *mtlOut)
{
	char *s;
	char *sNext;
	char **mat;
	int matLast;
	int matNum;
	int matNumNew;
	
	/* count, allocate, propagate array of 'newmtl' addresses
	 * (the last one points to the end of the mtl file)
	 */
	for (matNum = 0, s = strstr(mtl, "newmtl"); s; s = strstr(s + 1, "newmtl"))
		++matNum;
	mat = calloc(matNum + 1, sizeof(*mat));
	assert(mat);
	for (matNum = 0, s = strstr(mtl, "newmtl"); s; s = strstr(s + 1, "newmtl"))
		mat[matNum++] = s;
	mat[matNum] = mtl + strlen(mtl);
	matLast = matNum - 1;
	
	//for (i = 0; i < matNum; ++i)
	//	fprintf(stderr, "[%d] '%.*s'\n", i, (int)(mat[i+1] - mat[i]), mat[i]);
	
	/* write new mtl file with one 'newmtl' for every 'usemtl'
	 * (guarantees same ordering as in the mesh)
	 */
	for (matNumNew = 0, s = obj; s; s = strstr(s + 1, "usemtl"))
	{
		const char *matStrNext;
		const char *matStr;
		char name[1024];
		unsigned nameLen;
		int matIdx;
		
		if (s == obj)
			continue;
		
		/* step ahead to name, skip whitespace, copy string */
		s += strlen("usemtl");
		while (*s && (isblank(*s) || iscntrl(*s)))
			++s;
		nameLen = strcspn(s, "\r\n") + 1;
		assert(nameLen < sizeof(name) / sizeof(*name));
		memcpy(name, s, nameLen);
		name[nameLen] = '\0';
		//fprintf(stderr, "%s\n", name);
		
		/* search through list backwards */
		for (matIdx = matLast; matIdx > -1; --matIdx)
			if (strstr(mat[matIdx], name))
				break;
		assert(matIdx > -1);
		matStr = mat[matIdx];
		matStrNext = mat[matIdx + 1];
		
		/* skip original name */
		matStr += strcspn(matStr, "\r\n");
		
		/* write duplicate material using new string */
		fprintf(mtlOut, "newmtl mat%d%.*s", matNumNew, (int)(matStrNext - matStr), matStr);
		++matNumNew;
	}
	
	/* write new obj file with unique material names */
	for (sNext = 0, matNumNew = 0, s = obj; s; s = sNext)
	{
		sNext = nextline(s);
		
		if (*s == '#')
			continue;
		
		if (!memcmp(s, "usemtl", 6))
		{
			fprintf(objOut, "usemtl mat%d\n", matNumNew);
			++matNumNew;
			continue;
		}
		
		fprintf(objOut, "%.*s", (int)(sNext - s), s);
	}
	
	free(mat);
}

int main(int argc, char *argv[])
{
	const char *objFn = argv[1];
	const char *mtlFn = argv[2];
	FILE *objOut = 0;
	FILE *mtlOut = 0;
	char *obj;
	char *mtl;
	
	if (argc != 3)
	{
		fprintf(stderr, "args: mtlsort obj.obj mtl.mtl\n");
		fprintf(stderr, "WARNING: this program directly modifies\n");
		fprintf(stderr, "the files it processes, so keep backups!\n");
		return -1;
	}
	
	if (!(obj = loadfilestring(objFn)))
	{
		fprintf(stderr, "failed to read file '%s'\n", objFn);
		return -1;
	}
	
	if (!(mtl = loadfilestring(mtlFn)))
	{
		fprintf(stderr, "failed to read file '%s'\n", mtlFn);
		return -1;
	}
	
	if (!(objOut = fopen(objFn, "wb")))
	{
		fprintf(stderr, "failed to open file '%s' for writing\n", objFn);
		return -1;
	}
	
	if (!(mtlOut = fopen(mtlFn, "wb")))
	{
		fprintf(stderr, "failed to open file '%s' for writing\n", mtlFn);
		return -1;
	}
	
	mtlsort(obj, mtl, objOut, mtlOut);
	
	fclose(objOut);
	fclose(mtlOut);
	free(obj);
	free(mtl);
	return 0;
}

