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

struct strie_pair {
    int   crossed_word[2];
    int   crossed_word_letter[2];
    int   word_orient[2];   // 1 - horisontal; -1 - vertical
    int   word_coord[2][2]; // coordinates of the beginning of the word
    int   procreator;       // the number of the root word
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

int build_branch(const int wordnum, struct strie_pair *node);
struct strie_pair *check_pair(struct strie_pair *main_node, struct strie_pair *check_node);
int print_branch(const int wordnum, struct strie_pair *node);

int add_child(struct cross_elem *word, struct strie_pair *new_child)
{
    struct strie_pair *cur_child = NULL;

    if (!word)
        return 1;

    if (!word->firstchild)
    {
        word->firstchild = new_child;
        return 0;
    }

    cur_child = word->firstchild;
    while (cur_child->brother)
    {
        cur_child = cur_child->brother;
    }
    cur_child->brother = new_child;
    return 0;
}

/*
 * Take wordnum elements and find all crossings between them
 */
int build_pairs(int wordnum)
{
    int i, j, k, l;
    for (i = 0; i < wordnum; i++)
    {
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
                    // Allocate new children for word[i] and word[j]
                    for (l = i; l <= j; l = l - i + j)
                    {
                        words[l].childnum++;
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
                        schild->procreator = l;
                        schild->firstchild = NULL;
                        schild->brother    = NULL;
                        schild->parent     = NULL;
                        schild->available_first_children = (int *)calloc(wordnum, sizeof(int));
                        schild->available_first_children[l] = words[l].childnum;

                        // Add allocated child to the parent
                        add_child(&words[l], schild);
                    }

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
int build_branch(const int wordnum, struct strie_pair *main_node)
{
    int j, cur_word_num, checking_word_num;
    struct strie_pair *cur_node = main_node;
    struct strie_pair *tmp_node = NULL;
    struct strie_pair *schild   = NULL;
    struct strie_pair *latest_child = NULL;
    int *cur_available_first_children = NULL;

    if (!main_node)
        return 0;

    cur_available_first_children = (int *)calloc(wordnum, sizeof(int));
    memcpy(cur_available_first_children, \
            main_node->available_first_children, \
            sizeof(int) * wordnum);

    // cur_node  - the node, which participants we are trying to scan
    // main_node - the node _to_ which we are trying to add these participants
    while (cur_node)
    {
        for (cur_word_num = 0; cur_word_num < 2; cur_word_num++)
        {
            // The global index of the word to check
            checking_word_num = cur_node->crossed_word[cur_word_num];
            // We shouldn't allow to search previous words for pairs
            if (checking_word_num < cur_node->procreator)
                break;

            if (!(tmp_node = words[checking_word_num].firstchild))
                break;

            for (j = 0; j < cur_available_first_children[checking_word_num]; j++)
                tmp_node = tmp_node->brother;

            while (tmp_node)
            {
                cur_available_first_children[checking_word_num]++;
                if (NULL != (schild = check_pair(main_node, tmp_node)))
                {
                    // Add new child to main_node
                    schild->parent = main_node;
                    schild->available_first_children = (int *)calloc(wordnum, sizeof(int));
                    memcpy(schild->available_first_children, \
                            cur_available_first_children, \
                            sizeof(int) * wordnum);
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
                tmp_node = tmp_node->brother;
            }
        }
        cur_node = cur_node->parent;
    }

    if (cur_available_first_children)
        free(cur_available_first_children);

    // Go down or to the brother or to the first !NULL parent's brother
    if (main_node->firstchild)
    {
        build_branch(wordnum, main_node->firstchild);
    }
    else if (main_node->brother)
    {
        build_branch(wordnum, main_node->brother);
    }
    else if (main_node->parent)
    {
        // Find first parent's brother != NULL
        tmp_node = main_node->parent;
        while (tmp_node && !tmp_node->brother)
            tmp_node = tmp_node->parent;
        if (tmp_node)
        {
            build_branch(wordnum, tmp_node->brother);
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

    // First we need to check whether the same pair already exists in one of
    // main_node's children
    cur_node = main_node->firstchild;
    while (cur_node)
    {
        if ((cur_node->crossed_word[0] == check_node->crossed_word[0]) && \
            (cur_node->crossed_word[1] == check_node->crossed_word[1]) && \
            (cur_node->crossed_word_letter[0] == check_node->crossed_word_letter[0]) && \
            (cur_node->crossed_word_letter[1] == check_node->crossed_word_letter[1]))
        {
            return NULL;
        }
        cur_node = cur_node->brother;
    }
    // Now check if its the same as one of top node's elder brothers
    // Note, that this check is not necessary - its removal will not lead to
    // great 'wrong' branch growth. If the current branch is wrong, it won't
    // last long after adding one of top node's elder brothers. So this check
    // may be removed if it wastes too much time.
    cur_node = main_node;
    while (cur_node->parent)
        cur_node = cur_node->parent;
    if ((cur_node->crossed_word[0] == check_node->crossed_word[0]) && \
            (cur_node->crossed_word[1] >= check_node->crossed_word[1]))
    {
        return 0;
    }
    // Check current main node and all its parents
    cur_node = main_node;
    while (cur_node)
    {
        // There can be only one pair of certain words. I.e. word 'i' cann't
        // cross the word 'j' in two places.
        if (((cur_node->crossed_word[0] == check_node->crossed_word[0]) && \
            (cur_node->crossed_word[1] == check_node->crossed_word[1])) || \
            (check_node->crossed_word[0] < cur_node->procreator) || \
            (check_node->crossed_word[1] < cur_node->procreator))
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
                        // allocate new child
                        schild = (struct strie_pair*)calloc(1, sizeof(struct strie_pair));
                        memcpy(schild, check_node, sizeof(struct strie_pair));
                        schild->procreator = main_node->procreator;
                        schild->brother = NULL; // because its the brother of the original pair
                        schild->word_orient[j] = cur_node->word_orient[i];
                        schild->word_orient[(j - 1) * (j - 1)] = -(schild->word_orient[j]);
                    }
                    // schild now exists, we need to verify orientation and MINDISTANCE
                    dist = cur_node->crossed_word_letter[i] - check_node->crossed_word_letter[j];
                    dist = dist < 0 ? -dist : dist;
                    if ((dist < MINDISTANCE) || (cur_node->word_orient[i] != schild->word_orient[j]))
                    {
                        free(schild);
                        schild = NULL;
                        return schild;
                    }
                    found = 1;
                }
            }
        }

        cur_node = cur_node->parent;
    }

    return schild;
}

int print_branch(const int wordnum, struct strie_pair *node)
{
    int i;
    struct strie_pair *tmp_node = NULL;

    if (!node)
        return 0;

    printf("Crossed words: %d, %d\n", node->crossed_word[0], node->crossed_word[1]);
    printf("Crossed words letters: %d, %d\n", node->crossed_word_letter[0], node->crossed_word_letter[1]);
    printf("Orient: %d, %d\n", node->word_orient[0], node->word_orient[1]);
    printf("Word coords: [%d, %d]; [%d, %d]\n", node->word_coord[0][0], node->word_coord[0][1], node->word_coord[1][0], node->word_coord[1][1]);

    // Go down or to the brother or to the first !NULL parent's brother
    if (node->firstchild)
    {
        printf("\nCHILD:\n");
        print_branch(wordnum, node->firstchild);
    }
    else if (node->brother)
    {
        printf("\nBROTHER:\n");
        print_branch(wordnum, node->brother);
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
            print_branch(wordnum, tmp_node->brother);
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
    for (i = 0; i < wordnum; i++)
        build_branch(wordnum, words[i].firstchild);

    for (i = 0; i < wordnum; i++)
    {
        printf("\n--------------------------------\nword[%d]=%s\n--------------------------------\n", i, words[i].word);
        print_branch(wordnum, words[i].firstchild);
    }

    return 0;
}
