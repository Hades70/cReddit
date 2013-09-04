#ifndef _REDDIT_LINK_C_
#define _REDDIT_LINK_C_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "global.h"
#include "link.h"
#include "token.h"
#include "jsmn.h"

/*
 * Allocates an empty reddit_link structure
 */
RedditLink *reddit_link_new()
{
    RedditLink *link = rmalloc(sizeof(RedditLink));

    /* Just initalize everything as NULL or zero */
    memset(link, 0, sizeof(RedditLink));

    return link;
}

/*
 * Frees a single reddit_link structure
 * Note: Doesn't free the reddit_link at 'link->next'
 */
void reddit_link_free (RedditLink *link)
{
    free(link->selftext);
    free(link->id);
    free(link->permalink);
    free(link->author);
    free(link->url);
    free(link->title);

    free(link);
}

/*
 * Allocates an empty reddit_link_list structure
 */
RedditLinkList *reddit_link_list_new()
{
    RedditLinkList *list = rmalloc(sizeof(RedditLinkList));
    memset(list, 0, sizeof(RedditLinkList));
    return list;
}

/*
 * Frees the 'reddit_list' linked-list from a reddit_link_list, freeing it to be used again without
 * clearing the list's info
 */
void reddit_link_list_free_links (RedditLinkList *list)
{
    RedditLink *link, *tmp;
    for (link = list->first; link != NULL; link = tmp) {
        tmp = link->next;
        reddit_link_free(link);
    }
    list->linkCount = 0;
    list->first = NULL;
    list->last = NULL;
}

/*
 * Fress a reddit_link_list as well as free's all reddit_link structures attached.
 */
void reddit_link_list_free (RedditLinkList *list)
{
    reddit_link_list_free_links(list);
    free(list->subreddit);
    free(list->modhash);
    free(list);
}

void reddit_link_list_add_link (RedditLinkList *list, RedditLink *link)
{
    list->linkCount++;
    if (list->last == NULL) {
        list->first = link;
        list->last  = link;
    } else {
        list->last->next = link;
        list->last = link;
    }
}

RedditLink *reddit_get_link(TokenParser *parser)
{
    RedditLink *link = reddit_link_new();

    TokenIdent ids[] = {
        ADD_TOKEN_IDENT_STRING("selftext",      link->selftext),
        ADD_TOKEN_IDENT_STRING("title",         link->title),
        ADD_TOKEN_IDENT_STRING("id",            link->id),
        ADD_TOKEN_IDENT_STRING("permalink",     link->permalink),
        ADD_TOKEN_IDENT_STRING("author",        link->author),
        ADD_TOKEN_IDENT_STRING("url",           link->url),
        ADD_TOKEN_IDENT_INT   ("score",         link->score),
        ADD_TOKEN_IDENT_INT   ("downs",         link->downs),
        ADD_TOKEN_IDENT_INT   ("ups",           link->ups),
        ADD_TOKEN_IDENT_INT   ("num_comments",  link->numComments),
        ADD_TOKEN_IDENT_INT   ("num_reports",   link->numReports),
        ADD_TOKEN_IDENT_BOOL  ("is_self",       link->flags, REDDIT_LINK_IS_SELF),
        ADD_TOKEN_IDENT_BOOL  ("over_18",       link->flags, REDDIT_LINK_OVER_18),
        ADD_TOKEN_IDENT_BOOL  ("clicked",       link->flags, REDDIT_LINK_CLICKED),
        ADD_TOKEN_IDENT_BOOL  ("stickied",      link->flags, REDDIT_LINK_STICKIED),
        ADD_TOKEN_IDENT_BOOL  ("edited",        link->flags, REDDIT_LINK_EDITED),
        ADD_TOKEN_IDENT_BOOL  ("hidden",        link->flags, REDDIT_LINK_HIDDEN),
        ADD_TOKEN_IDENT_BOOL  ("distinguished", link->flags, REDDIT_LINK_DISTINGUISHED),
        {0}
    };

    parse_tokens(parser, ids);

    return link;
}

#define ARG_LIST_GET_LISTING \
    RedditLinkList *list = va_arg(args, RedditLinkList*);

DEF_TOKEN_CALLBACK(get_listing_helper)
{
    ARG_LIST_GET_LISTING


    /* The below simply loops through all the current idents, finds the 'kind' key
     * and checks if it's a 't3'. If it is t3, then it's another link and we call
     * 'reddit_get_link' to parse the data and give us our link */
    int i;
    for (i = 0; idents[i].name != NULL; i++)
        if (strcmp(idents[i].name, "kind") == 0)
            if (idents[i].value != NULL && strcmp(*((char**)idents[i].value), "t3") == 0)
                reddit_link_list_add_link(list, reddit_get_link(parser));

}

/*
 * Gets the contents of a subreddit and adds them to 'list' -- If list already has elements in it
 * it will ask reddit to give us the next links in the list
 *
 * 'subreddit' should be in the form '/r/subreddit' or empty to indicate 'front'
 */
RedditErrno reddit_get_listing (RedditLinkList *list)
{
    char subred[1024], *kind_str = NULL;
    TokenParserResult res;

    TokenIdent ids[] = {
        ADD_TOKEN_IDENT_STRING("modhash", list->modhash),
        ADD_TOKEN_IDENT_STRING("kind", kind_str),
        ADD_TOKEN_IDENT_FUNC("data", get_listing_helper),
        {0}
    };

    strcpy(subred, "http://www.reddit.com");
    if (list->subreddit != NULL)
        strcat(subred, list->subreddit);

    if (list->type == REDDIT_NEW)
        strcat(subred, "/new");
    else if (list->type == REDDIT_RISING)
        strcat(subred, "/rising");
    else if (list->type == REDDIT_CONTR)
        strcat(subred, "/controversial");
    else if (list->type == REDDIT_TOP)
        strcat(subred, "/top");

    strcat(subred, "/.json");

    if (list->linkCount > 0)
        sprintf(subred + strlen(subred), "?after=t3_%s", list->last->id);

    res = reddit_run_parser(subred, NULL, ids, list);

    free(kind_str);

    if (res == TOKEN_PARSER_SUCCESS)
        return REDDIT_SUCCESS;
    else
        return REDDIT_ERROR_RESPONSE;
}


#endif
