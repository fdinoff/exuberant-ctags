/*
*   $Id$
*
*   Copyright (c) 2011, Ivan Krasilnikov
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module implements parsing of protocol buffers definition files
*   (http://code.google.com/apis/protocolbuffers/docs/proto.html)
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>
#include <ctype.h>

#include "entry.h"
#include "parse.h"
#include "get.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/

typedef enum {
    PK_PACKAGE,
    PK_MESSAGE,
    PK_FIELD,
    PK_ENUMERATOR,
    PK_ENUM,
    PK_SERVICE,
    PK_RPC
} protobufKind;

static kindOption ProtobufKinds [] = {
	{ TRUE,  'p', "package",    "packages" },
	{ TRUE,  'm', "message",    "messages" },
	{ TRUE,  'f', "field",      "fields" },
	{ TRUE,  'e', "enumerator", "enum constants" },
	{ TRUE,  'g', "enum",       "enum types" },
	{ TRUE,  's', "service",    "services" },
	{ FALSE, 'r', "rpc",        "RPC methods" }
};

#define TOKEN_EOF   0
#define TOKEN_ID    'i'

static struct sTokenInfo {
    int type;         /* one of TOKEN_* constants or punctuation characters */
    vString *value;
} token;

/*
*   FUNCTION DEFINITIONS
*/

static void nextToken (void)
{
    int c;

repeat:
    /*
     * .proto files may contain C and C++ style comments and
     * quoted strings. cppGetc() takes care of them.
     */
    c = cppGetc ();

    if (c <= 0)
        token.type = TOKEN_EOF;
    else if (c == '{' || c == '}' || c == ';' || c == '.' || c == '=')
        token.type = c;
    else if (isalnum (c) || c == '_')
    {
        token.type = TOKEN_ID;
        vStringClear (token.value);
        while (c > 0 && (isalnum (c) || c == '_')) {
            vStringPut (token.value, c);
            c = cppGetc ();
        }
        cppUngetc (c);
    }
    else
        goto repeat;  /* anything else is not important for this parser */
}

static void skipUntil (const char *punctuation)
{
    while (token.type != TOKEN_EOF && strchr (punctuation, token.type) == NULL)
        nextToken ();
}

static int tokenIsKeyword(const char *keyword)
{
    return token.type == TOKEN_ID && strcmp (vStringValue (token.value), keyword) == 0;
}

static void createProtobufTag (const vString *name, int kind)
{
    static tagEntryInfo tag;

	if (ProtobufKinds [kind].enabled)
    {
	    initTagEntry (&tag, vStringValue (name));
        tag.kindName = ProtobufKinds [kind].name;
	    tag.kind     = ProtobufKinds [kind].letter;
        makeTagEntry (&tag);
    }
}

static void parseEnumConstants (void)
{
    if (token.type != '{')
        return;
    nextToken ();

    while (token.type != TOKEN_EOF && token.type != '}')
    {
        if (token.type == TOKEN_ID && !tokenIsKeyword ("option"))
        {
            nextToken ();  /* doesn't clear token.value if it's punctuation */
            if (token.type == '=')
                createProtobufTag (token.value, PK_ENUMERATOR);
        }

        skipUntil (";}");

        if (token.type == ';')
            nextToken ();
    }
}

static void parseStatement (int kind)
{
    nextToken ();

    if (kind == PK_FIELD)
    {
        /* skip field's type */
        do
        {
            if (token.type == '.')
                nextToken ();
            if (token.type != TOKEN_ID)
                return;
            nextToken ();
        } while (token.type == '.');
    }

    if (token.type != TOKEN_ID)
        return;

    createProtobufTag (token.value, kind);
    nextToken ();

    if (kind == PK_ENUM)
        parseEnumConstants ();
}

static void findProtobufTags (void)
{
	cppInit (FALSE, FALSE);
    token.value = vStringNew ();

    nextToken ();

    while (token.type != TOKEN_EOF)
    {
        if (tokenIsKeyword ("package"))
            parseStatement (PK_PACKAGE);
        else if (tokenIsKeyword ("message"))
            parseStatement (PK_MESSAGE);
        else if (tokenIsKeyword ("enum"))
            parseStatement (PK_ENUM);
        else if (tokenIsKeyword ("repeated") || tokenIsKeyword ("optional") || tokenIsKeyword ("required"))
            parseStatement (PK_FIELD);
        else if (tokenIsKeyword ("service"))
            parseStatement (PK_SERVICE);
        else if (tokenIsKeyword ("rpc"))
            parseStatement (PK_RPC);

        skipUntil (";{}");
        nextToken ();
    }

    vStringDelete (token.value);
	cppTerminate ();
}

extern parserDefinition* ProtobufParser (void)
{
	static const char *const extensions [] = { "proto", NULL };
	parserDefinition* def = parserNew ("Protobuf");
	def->extensions = extensions;
	def->kinds      = ProtobufKinds;
	def->kindCount  = KIND_COUNT (ProtobufKinds);
	def->parser     = findProtobufTags;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
