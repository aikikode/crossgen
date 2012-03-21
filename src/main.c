/*
 * Crossword Generator main file
 *
 * Copyright (C) 2012 Denis Kovalev (aikikode@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXWORDS    1000  // maximum number of words to create a crossword from
#define MAXWORDLEN  30

#define MINDISTANCE 2     /* minimum distance between the crossing words, e.g.
                           *        0123456
                           *        |||||||
                           *        license
                           *         n    m
                           *         d<-->a
                           *         i    i
                           *         a    l
                           *
                           * in this example the distance between the words is:
                           * 6 - 1 = 5
                           */
#define DEBUG       1

// The elements structures are at initial state for now. Most probably they'll
// change in the future.

struct strie_pair {
    int   crossed_word[2];
    int   crossed_word_letter[2];
    int   childnum;       // number of all children of the current node
    int   word_orient[2];   // 1 - horisontal; -1 - vertical
    int   word_coord[2][2]; // coordinates of the beginning of the word
    int  *available_first_children; /* an array of len 'wordnum' that holds the
                                       numbers of each word first pair that can
                                       be analyzed to create a child */
    struct strie_pair *firstchild;
    struct strie_pair *brother;
    struct strie_pair *parent;
};

struct cross_elem {
    char  word[MAXWORDLEN];
    int   wordlen;
    int   childnum;    // number of pairs between this words and later words
    struct strie_pair *firstchild;
};

struct cross_elem words[MAXWORDS];

int fill_strie(const int wordnum, struct strie_pair *node);
struct strie_pair *check_pair(struct strie_pair *main_node, struct strie_pair *check_node);
int print_strie(const int wordnum, struct strie_pair *node);

/*
 * Take wordnum elements and find all crossings between them
 */
int build_pairs(int wordnum)
{
    int i, j, k;
    for (i = 0; i < wordnum - 1; i++)
    {
        // When we add new pair we should know what the latest one was to
        // add just after it (add its brother)
        struct strie_pair *latest_child = NULL;

        for (j = i + 1; j < wordnum; j++)
        {
            // Find all crossings between words 'i' and 'j'
            for (k = 0; k < words[i].wordlen; k++)
            {
                struct strie_pair *schild = NULL;
                char *tmp_wordpart = words[j].word;
                while (NULL != (tmp_wordpart = strchr(tmp_wordpart, words[i].word[k])))
                {
#if DEBUG
                    printf("crossing between %s and %s: %c, %d, %d\n", \
                            words[i].word, \
                            words[j].word, \
                            tmp_wordpart[0], \
                            k, \
                            tmp_wordpart - words[j].word);
#endif
                    words[i].childnum++;
                    // Allocate new child for word[i]
                    if (!(schild = (struct strie_pair*)calloc(1, sizeof(struct strie_pair))))
                    {
                        fprintf(stderr, "Not enough memory!\n");
                        return 1;
                    }
                    schild->crossed_word[0]    = i;
                    schild->crossed_word[1]    = j;
                    schild->crossed_word_letter[0] = k;
                    schild->crossed_word_letter[1] = tmp_wordpart - words[j].word;
                    schild->word_orient[0] = 1;
                    schild->word_orient[1] = -(schild->word_orient[0]);
                    schild->word_coord[0][0] = 0;
                    schild->word_coord[0][1] = 0;
                    schild->word_coord[1][0] = schild->crossed_word_letter[0];
                    schild->word_coord[1][1] = schild->crossed_word_letter[1];
                    schild->childnum   = 0;
                    schild->firstchild = NULL;
                    schild->brother    = NULL;
                    schild->parent     = NULL;
                    schild->available_first_children = (int *)calloc(wordnum - 1, sizeof(int));
                    schild->available_first_children[i] = words[i].childnum;

                    // Add allocated child to the parent either as the first
                    // child or add the brother to the latest child
                    if (NULL == words[i].firstchild || NULL == latest_child)
                    {
                        words[i].firstchild = schild;
                    }
                    else
                    {
                        latest_child->brother = schild;
                    }
                    latest_child = schild;

                    tmp_wordpart++;
                }
            }
        }
    }
    return 0;
}

/* Its goal is to add all children to the current node, move the current node
 * pointer and recursively call itself.
 */
int fill_strie(const int wordnum, struct strie_pair *main_node)
{
    int i, j, cur_word_num, checking_word_num;
    struct strie_pair *cur_node = main_node;
    struct strie_pair *tmp_node = NULL;
    struct strie_pair *schild   = NULL;
    struct strie_pair *latest_child = NULL;
    int *cur_available_first_children = NULL;

    if (!main_node)
        return 0;

    cur_available_first_children = (int *)calloc(wordnum - 1, sizeof(int));
    memcpy(cur_available_first_children, \
            main_node->available_first_children, \
            sizeof(int) * (wordnum - 1));

    while (cur_node)
    {
        for (cur_word_num = 0; cur_word_num < 2; cur_word_num++)
        {
            // The global index of the word to check
            checking_word_num = cur_node->crossed_word[cur_word_num];
            tmp_node = words[checking_word_num].firstchild;
            for (j = 0; j < cur_node->available_first_children[checking_word_num]; j++)
                tmp_node = tmp_node->brother;

            for (i = cur_node->available_first_children[checking_word_num]; \
                    i < words[checking_word_num].childnum; \
                    i++, tmp_node = tmp_node->brother)
            {
                if (!tmp_node)
                    break;

                cur_available_first_children[checking_word_num]++;
                if (NULL != (schild = check_pair(main_node, tmp_node)))
                {
                    main_node->childnum++;
                    // Add new child to main_node
                    schild->parent = main_node;
                    memcpy(schild->available_first_children, \
                            cur_available_first_children, \
                            sizeof(int) * (wordnum - 1));
                    // Add child to the parent either as the first
                    // child or add the brother to the latest child
                    if (NULL == main_node->firstchild || NULL == latest_child)
                    {
                        main_node->firstchild = schild;
                    }
                    else
                    {
                        latest_child->brother = schild;
                    }
                    latest_child = schild;
                }
            }
        }
        cur_node = cur_node->parent;
    }

    // Go down or to the brother or to the first !NULL parent's brother
    if (main_node->firstchild)
    {
        fill_strie(wordnum, main_node->firstchild);
    }
    else if (main_node->brother)
    {
        fill_strie(wordnum, main_node->brother);
    }
    else if (main_node->parent)
    {
        // Find first parent's brother != NULL
        tmp_node = main_node->parent;
        while (tmp_node && !tmp_node->brother)
            tmp_node = tmp_node->parent;
        if (tmp_node)
        {
            fill_strie(wordnum, tmp_node->brother);
        }
        else
        {
            // We've processed all tree for one crossword word
            return 0;
        }
    }
    // This code is reached only when all pairs for the word have been
    // processed
    return 0;
}

struct strie_pair *check_pair(struct strie_pair *main_node, struct strie_pair *check_node)
{
    struct strie_pair *cur_node = main_node;
    struct strie_pair *schild = NULL;
    int i, j, dist, found;

    // Check current main node and all its parents
    while (cur_node)
    {
        // There can be only one pair of certain words. I.e. word 'i' cann't
        // cross the word 'j' in two places.
        if (    (cur_node->crossed_word[0] == check_node->crossed_word[0]) && \
                (cur_node->crossed_word[1] == check_node->crossed_word[1]))
        {
            if (schild)
            {
                free(schild);
                schild = NULL;
            }
            return schild;
        }
        // Find same word in two pairs
        found = 0;
        for (i = 0; i < 2 && !found; i++)
        {
            for (j = 0; j < 2 && !found; j++)
            {
                if (cur_node->crossed_word[i] == check_node->crossed_word[j])
                {
                    if (!schild)
                    {
                        schild = (struct strie_pair*)calloc(1, sizeof(struct strie_pair));
                        memcpy(schild, check_node, sizeof(struct strie_pair));
                        schild->brother = NULL; // because its the brother of the original pair
                        schild->word_orient[j] = cur_node->word_orient[i];
                        schild->word_orient[(j - 1) * (j - 1)] = -(schild->word_orient[j]);
                        // Check the allowed distance between the words
                        dist = cur_node->crossed_word_letter[i] - check_node->crossed_word_letter[j];
                        dist = dist < 0 ? -dist : dist;
                        if (dist < MINDISTANCE)
                        {
                            free(schild);
                            schild = NULL;
                            return schild;
                        }
                    }
                    else
                    {
                        // schild alreaddy exists, we need to verify orientation and MINDISTANCE
                        dist = cur_node->crossed_word_letter[i] - check_node->crossed_word_letter[j];
                        dist = dist < 0 ? -dist : dist;
                        if ((dist < MINDISTANCE) || (cur_node->word_orient[i] != schild->word_orient[j]))
                        {
                            free(schild);
                            schild = NULL;
                            return schild;
                        }
                    }
                    found = 1;
                }
            }
        }

        cur_node = cur_node->parent;
    }

    return schild;
}

int print_strie(const int wordnum, struct strie_pair *node)
{
    int i;
    struct strie_pair *tmp_node = NULL;

    if (!node)
        return 0;

    printf("Crossed words: %d, %d\n", node->crossed_word[0], node->crossed_word[1]);
    printf("Crossed words letters: %d, %d\n", node->crossed_word_letter[0], node->crossed_word_letter[1]);
    printf("Childnum: %d\n", node->childnum);
    printf("Orient: %d, %d\n", node->word_orient[0], node->word_orient[1]);
    printf("Word coords: [%d, %d]; [%d, %d]\n", node->word_coord[0][0], node->word_coord[0][1], node->word_coord[1][0], node->word_coord[1][1]);

    // Go down or to the brother or to the first !NULL parent's brother
    if (node->firstchild)
    {
        printf("\nCHILD:\n");
        print_strie(wordnum, node->firstchild);
    }
    else if (node->brother)
    {
        printf("\nBROTHER:\n");
        print_strie(wordnum, node->brother);
    }
    else if (node->parent)
    {
        // Find first parent's brother != NULL
        tmp_node = node->parent;
        i = 1;
        while (tmp_node && !tmp_node->brother)
        {
            tmp_node = tmp_node->parent;
            i++;
        }
        if (tmp_node)
        {
            printf("\nPARENT #%d BROTHER:\n", i);
            print_strie(wordnum, tmp_node->brother);
        }
        else
        {
            // We've processed all tree for one crossword word
            return 0;
        }
    }
 
    return 0;
}

int main(int argc, char **argv)
{
    int i, wordnum = 0;

    // Input the words to build the crossword from and store them in 'words'
    // structs
    printf("Welcome to Crossword Generator\n==============================\n");
    printf("How many words would you like to enter: ");
    scanf("%d", &wordnum);
    for (i = 0; i < wordnum; i++)
    {
        printf("Input word #%d: ", i + 1);
        scanf("%s", words[i].word);
        words[i].wordlen = strlen(words[i].word);
    }

    // Build inital word pairs that we'll be using a lot later
    if (build_pairs(wordnum))
    {
        fprintf(stderr, "Error building pairs between words\n");
        return 1;
    }
    // Fill the tree with all possible pairs. Scan all the words but one
    for (i = 0; i < wordnum - 1; i++)
        fill_strie(wordnum, words[i].firstchild);

    for (i = 0; i < wordnum - 1; i++)
    {
        printf("\nword[%d]=%s\n", i, words[i].word);
        print_strie(wordnum, words[i].firstchild);
    }

    return 0;
}
