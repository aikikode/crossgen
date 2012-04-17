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

// Timer requires:
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Custom signal handler requires:
#include <signal.h>

// kill() function requires:
#include <sys/types.h>
#include <unistd.h>

#define MAXWORDS    100   // maximum number of words to create a crossword from
#define MAXWORDLEN  15

#define MINDISTANCE 2     /* minimum distance between the crossing words, e.g.
                           *        0123456
                           *        |||||||
                           *        license
                           *         n    m
                           *         d<-->a
                           *         i    i
                           *         a    l
                           *
                           * in this example the distance between the words is
                           * 6 - 1 = 5
                           */
struct strie_pair {
    short   crossed_word[2];
    short   crossed_word_letter[2];
    short   word_orient[2];   // 1 - horisontal; -1 - vertical
    short   word_coord[2][2]; // coordinates of the beginning of the word
    short   procreator;       // the number of the root word
    short   depth;
    short  *available_first_children; /* an array of len 'wordnum' that holds the
                                       numbers of each word first pair that can
                                       be analyzed to create a child */
    struct strie_pair *firstchild;
    struct strie_pair *brother;
    struct strie_pair *parent;
};

struct cross_elem {
    char    word[MAXWORDLEN];
    short   wordlen;
    short   childnum;    // number of pairs between this words and later words
    struct  strie_pair *firstchild;
};

struct cross_elem words[MAXWORDS];
struct strie_pair *best_branch = NULL;
struct strie_pair *common_parent = NULL;

int build_branch(const short wordnum, struct strie_pair *node, int limit);
struct strie_pair *check_pair(struct strie_pair *main_node, struct strie_pair *check_node);
int print_strie(struct strie_pair *node);
int print_branch(struct strie_pair *node);
int clear_branch(struct strie_pair *node);

enum error_codes {
    BUILD_OK = 0,
    BUILD_FINISHED
};

int sigint_called = 0;

void sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        sigint_called++;
        // Print the best branch if any
        print_branch(best_branch);
        printf("Hit Ctrl-C again to exit the program\n");
        if (sigint_called > 1)
            kill(getpid(), SIGKILL);
    }
}

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
int build_pairs(short wordnum)
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
#ifdef DEBUG
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
                        schild->depth = 0;
                        schild->procreator = l;
                        schild->firstchild = NULL;
                        schild->brother    = NULL;
                        schild->parent     = NULL;
                        schild->available_first_children = (short *)calloc(wordnum, sizeof(short));
                        schild->available_first_children[l] = words[l].childnum;
                        if (l == i)
                            schild->available_first_children[j] = words[j].childnum + 1;

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
 * pointer and recursively continue
 */
int build_branch(const short wordnum, struct strie_pair *main_node, int limit)
{
    int j;
    short  cur_word_num, checking_word_num;
    struct strie_pair *cur_node = main_node;
    struct strie_pair *tmp_node = NULL;
    struct strie_pair *schild   = NULL;
    short *cur_available_first_children = (short *)calloc(wordnum, sizeof(short));

    while (main_node)
    {
        schild = NULL;
        memset(cur_available_first_children, '\0', sizeof(short));
        memcpy(cur_available_first_children, \
                main_node->available_first_children, \
                sizeof(short) * wordnum);

        // cur_node  - the node, which participants we are trying to scan
        // main_node - the node _to_ which we are trying to add these participants
        cur_node = main_node;
        while (cur_node && !schild)
        {
            for (cur_word_num = 0; cur_word_num < 2; cur_word_num++)
            {
                // The global index of the word to check
                checking_word_num = cur_node->crossed_word[cur_word_num];

                // We shouldn't allow to search previous words for pairs
                if (checking_word_num < cur_node->procreator)
                    break;

                // Skip this word if we've already processed all its children
                if (cur_available_first_children[checking_word_num] == words[checking_word_num].childnum)
                    continue;

                // We have nothing to check if the current word doesn't have
                // any children
                if (!(tmp_node = words[checking_word_num].firstchild))
                    continue;

                // Get to the first child we can start iterate from
                for (j = 0; j < cur_available_first_children[checking_word_num]; j++)
                    tmp_node = tmp_node->brother;

                // Switch words crossed pairs
                while (tmp_node && !schild)
                {
                    cur_available_first_children[checking_word_num]++;
                    if (NULL != (schild = check_pair(main_node, tmp_node)))
                    {
                        // Add new child to main_node
                        schild->parent = main_node;
                        schild->available_first_children = (short *)calloc(wordnum, sizeof(short));
                        memcpy(schild->available_first_children, \
                                cur_available_first_children, \
                                sizeof(short) * wordnum);
                        // Also update the main node 'available children'
                        memcpy(main_node->available_first_children, \
                                cur_available_first_children, \
                                sizeof(short) * wordnum);
                        // Add child to the parent either as the first
                        // child or add the brother to the first child
                        if (NULL == main_node->firstchild)
                        {
                            main_node->firstchild = schild;
                        }
                        else
                        {
                            // If we are adding a brother - main_node is a 'common_parent'
                            main_node->firstchild->brother = schild;
                            common_parent = main_node;
                        }
                    }
                    tmp_node = tmp_node->brother;
                }
            }
            cur_node = cur_node->parent;
        }

        if (schild)
        {
            // If we created a new child go to it
            main_node = schild;
        }
        else
        {
            // Is the current node - leaf node?
            if (!main_node->firstchild)
            {
                // If it's the first finished branch it's the best branch
                if (!best_branch)
                {
                    best_branch = main_node;
                    sigint_called = 0;
                    if (limit)
                    {
                        if (best_branch->depth >= limit)
                        {
                            if (cur_available_first_children)
                                free(cur_available_first_children);
                            return BUILD_FINISHED;
                        }
                    }
                    printf("\nGenerated crossword with %d crossings. Hit Ctrl-C to view it.\n", best_branch->depth + 1);

                    if (main_node->parent)
                    {
                        main_node = main_node->parent;
                    }
                    else
                    {
                        // We climbed up to 'main pairs'. No need to check
                        // brother for NULL.
                        main_node = main_node->brother;
                    }
                }
                else
                {
                    // If the best branch already exists...
                    if (main_node->depth > best_branch->depth)
                    {
                        // We found new best branch - should remove old one and
                        // free memory of all nodes below the 'common_parent'
                        // if any
                        for (cur_node = best_branch; cur_node != common_parent && cur_node->parent; )
                        {
                            tmp_node = cur_node;
                            // If current node has a brother, now it becomes
                            // the elderst
                            if (cur_node->brother)
                            {
                                cur_node->parent->firstchild = cur_node->brother;
                            }
                            else
                            {
                                cur_node->parent->firstchild = NULL;
                            }
                            cur_node = cur_node->parent;

                            if (tmp_node->available_first_children)
                                free(tmp_node->available_first_children);
                            free(tmp_node);
                        }
                        common_parent = NULL;
                        best_branch = main_node;
                        sigint_called = 0;
                        if (limit)
                        {
                            if (best_branch->depth >= limit)
                            {
                                if (cur_available_first_children)
                                    free(cur_available_first_children);
                                return BUILD_FINISHED;
                            }
                        }
                        printf("Generated crossword with %d crossings. Hit Ctrl-C to view it.\n", best_branch->depth + 1);

                        if (main_node->parent)
                        {
                            main_node = main_node->parent;
                        }
                        else
                        {
                            // We climbed up to 'main pairs'. No need to check
                            // brother for NULL.
                            main_node = main_node->brother;
                        }
                    }
                    else
                    {
                        // Current 'main_node' branch is not better than the
                        // 'best_branch'. Remove current node and move to its
                        // parent.
                        if (main_node->parent)
                        {
                            cur_node = main_node;
                            main_node = main_node->parent;
                            if (cur_node->parent->firstchild == cur_node)
                            {
                                cur_node->parent->firstchild = NULL;
                            }
                            else
                            {
                                cur_node->parent->firstchild->brother = NULL;
                            }
                            if (cur_node->available_first_children)
                                free(cur_node->available_first_children);
                            free(cur_node);
                        }
                        else
                        {
                            // We climbed up to 'main pairs'. No need to check
                            // brother for NULL.
                            main_node = main_node->brother;
                        }
                    }
                }
            }
            else
            {
                // The current 'main_node' is not the leaf node and has no
                // other children - it means that it is one of the parents of
                // the best_branch node
                if (main_node->parent)
                {
                    main_node = main_node->parent;
                }
                else
                {
                    // We climbed up to 'main pairs'. No need to check
                    // brother for NULL.
                    main_node = main_node->brother;
                }
            }
        }
    }

    if (cur_available_first_children)
        free(cur_available_first_children);

    // This code is reached only when all pairs for the word have been
    // processed
    return BUILD_OK;
}

struct strie_pair *check_pair(struct strie_pair *main_node, struct strie_pair *check_node)
{
    struct strie_pair *cur_node = main_node;
    struct strie_pair *schild = NULL;
    struct strie_pair *tmp_node = NULL;
    short i, j, dist, found;

    // First we need to check whether the same pair already exists in one of
    // main_node's children
    for (cur_node = main_node->firstchild; cur_node; cur_node = cur_node->brother)
    {
        if ((cur_node->crossed_word[0] == check_node->crossed_word[0]) && \
            (cur_node->crossed_word[1] == check_node->crossed_word[1]) && \
            (cur_node->crossed_word_letter[0] == check_node->crossed_word_letter[0]) && \
            (cur_node->crossed_word_letter[1] == check_node->crossed_word_letter[1]))
        {
            return NULL;
        }
    }
    // Now check whether it's the same as one of top node's elder brothers
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
        return NULL;
    }
    // Check whether it's the same as one of main_node's or any its parent's
    // elder brothers
    for (cur_node = main_node; cur_node; cur_node = cur_node->parent)
    {
        if (cur_node->parent)
        {
            for (tmp_node = cur_node->parent->firstchild; \
                    tmp_node != cur_node; \
                    tmp_node = tmp_node->brother)
            {
                if ((tmp_node->crossed_word[0] == check_node->crossed_word[0]) && \
                        (tmp_node->crossed_word[1] == check_node->crossed_word[1]) && \
                        (tmp_node->crossed_word_letter[0] == check_node->crossed_word_letter[0]) && \
                        (tmp_node->crossed_word_letter[1] == check_node->crossed_word_letter[1]))
                {
                    return NULL;
                }
            }
        }
    }

    // Check current main node and all its parents
    for (cur_node = main_node; cur_node; cur_node = cur_node->parent)
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
                        short dx, dy, tmp;
                        // allocate new child
                        schild = (struct strie_pair*)calloc(1, sizeof(struct strie_pair));
                        memcpy(schild, check_node, sizeof(struct strie_pair));
                        schild->depth = main_node->depth + 1;
                        schild->procreator = main_node->procreator;
                        schild->brother = NULL; // because it's the brother of the original pair
                        if (schild->word_orient[j] != cur_node->word_orient[i])
                        {
                            tmp = schild->word_coord[j][0];
                            schild->word_coord[j][0] = -schild->word_coord[j][1];
                            schild->word_coord[j][1] = -tmp;
                            tmp = schild->word_coord[(j - 1) * (j - 1)][0];
                            schild->word_coord[(j - 1) * (j - 1)][0] = -schild->word_coord[(j - 1) * (j - 1)][1];
                            schild->word_coord[(j - 1) * (j - 1)][1] = -tmp;
                        }
                        schild->word_orient[j] = cur_node->word_orient[i];
                        schild->word_orient[(j - 1) * (j - 1)] = -(schild->word_orient[j]);
                        dx = cur_node->word_coord[i][0] - schild->word_coord[j][0] ;
                        dy = cur_node->word_coord[i][1] - schild->word_coord[j][1] ;
                        schild->word_coord[0][0] += dx;
                        schild->word_coord[0][1] += dy;
                        schild->word_coord[1][0] += dx;
                        schild->word_coord[1][1] += dy;
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
    }

    // Detect whether our newly added word crosses other words in 'wrong' letters
    if (schild)
    {
        for (cur_node = main_node; cur_node; cur_node = cur_node->parent)
        {
            for (i = 0; i < 2; i++)
            {
                for (j = 0; j < 2; j++)
                {
                    short xa = cur_node->word_coord[i][0];
                    short ya = cur_node->word_coord[i][1];
                    short la = words[cur_node->crossed_word[i]].wordlen;
                    short xb = schild->word_coord[j][0];
                    short yb = schild->word_coord[j][1];
                    short lb = words[schild->crossed_word[j]].wordlen;
                    // If the new word is already in the crossword, its
                    // position should be the same
                    if (cur_node->crossed_word[i] == schild->crossed_word[j])
                    {
                        if ((cur_node->word_orient[i] != schild->word_orient[j]) || \
                                (xa != xb) || (ya != yb))
                        {
                            free(schild);
                            schild = NULL;
                            return schild;
                        }
                    }
                    else
                    {
                        // Check intersection based on orientation
                        if (1 == cur_node->word_orient[i])
                        {
                            if (1 == schild->word_orient[j])
                            {
                                // Both horizontal - they should have no intersections
                                dist = ya - yb;
                                dist = dist < 0 ? -dist : dist;
                                if (!((dist >= MINDISTANCE) || \
                                            (xb >= xa + la - 1 + MINDISTANCE) || \
                                            (xa >= xb + lb - 1 + MINDISTANCE)))
                                {
                                    free(schild);
                                    schild = NULL;
                                    return schild;
                                }
                            }
                            else
                            {
                                // The word in the crossword (a) - horizontal,
                                // the new word (b) - vertical
                                if (!((xa >= xb + MINDISTANCE) || \
                                        (xb >= xa + la - 1 + MINDISTANCE) || \
                                        (ya >= yb + MINDISTANCE) || \
                                        (ya <= yb - lb + 1 - MINDISTANCE)))
                                {
                                    // Determine the letter position in each word
                                    short apos = xb - xa;
                                    short bpos = yb - ya;
                                    if ((apos >= la) || (bpos >= lb) || \
                                            (words[cur_node->crossed_word[i]].word[apos] != words[schild->crossed_word[j]].word[bpos]))
                                    {
                                        free(schild);
                                        schild = NULL;
                                        return schild;
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (1 == schild->word_orient[j])
                            {
                                // Original (a) - vertical
                                // New (b) - horizontal
                                if (!((xb >= xa + MINDISTANCE) || \
                                            (xa >= xb + lb -1 + MINDISTANCE) || \
                                            (yb >= ya + MINDISTANCE) || \
                                            (yb <= ya - la + 1 - MINDISTANCE)))
                                {
                                    // Determine the letter position in each word
                                    short bpos = xa - xb;
                                    short apos = ya - yb;
                                    if ((apos >= la) || (bpos >= lb) || \
                                            (words[cur_node->crossed_word[i]].word[apos] != words[schild->crossed_word[j]].word[bpos]))
                                    {
                                        free(schild);
                                        schild = NULL;
                                        return schild;
                                    }
                                }
                            }
                            else
                            {
                                // Both are vertical - there should be no intersections
                                dist = xa - xb;
                                dist = dist < 0 ? -dist : dist;
                                if (!((dist >= MINDISTANCE) || \
                                            (yb <= ya - la + 1 - MINDISTANCE) || \
                                            (ya <= yb - lb + 1 - MINDISTANCE)))
                                {
                                    free(schild);
                                    schild = NULL;
                                    return schild;
                                }
                            }
                        }
                        // If we reach this code, it means that 'i' and 'j'
                        // words cross. Should detect whether this crossing is
                        // legal, i.e. crossed letter is the same in both words
                    }
                }
            }
        }
    }

    return schild;
}

int print_strie(struct strie_pair *node)
{
    int i;
    struct strie_pair *tmp_node = NULL;

    if (!node)
        return 0;

    printf("Crossed words: %d, %d\n", node->crossed_word[0], node->crossed_word[1]);
    printf("Crossed words letters: %d, %d\n", node->crossed_word_letter[0], node->crossed_word_letter[1]);
    printf("Orient: %d, %d\n", node->word_orient[0], node->word_orient[1]);
    printf("Depth: %d\n", node->depth);
    printf("Word coords: [%d, %d]; [%d, %d]\n", node->word_coord[0][0], node->word_coord[0][1], node->word_coord[1][0], node->word_coord[1][1]);

    // Go down or to the brother or to the first !NULL parent's brother
    if (node->firstchild)
    {
        printf("\nCHILD:\n");
        print_strie(node->firstchild);
    }
    else if (node->brother)
    {
        printf("\nBROTHER:\n");
        print_strie(node->brother);
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
            print_strie(tmp_node->brother);
        }
        else
        {
            // We've processed all tree for one crossword word
            return 0;
        }
    }

    return 0;
}

// Print the branch from the current node to the root
int print_branch(struct strie_pair *node)
{
    int i, index;
    struct strie_pair *cur_node = node;

    if (!node)
        return 0;

    printf("\nGenerated crossword puzzle with %d crossings:\n----------------------------------------------\n", cur_node->depth + 1);
    while (cur_node)
    {
        // Mark intersection letters uppercase
        for (i = 0; i < 2; i++)
        {
            index = cur_node->crossed_word_letter[i];
            words[cur_node->crossed_word[i]].word[index] = \
                toupper(words[cur_node->crossed_word[i]].word[index]);
        }
        printf("Crossed words:\t%d, %s\t-\t%d, %s\n", \
                cur_node->crossed_word[0], \
                words[cur_node->crossed_word[0]].word, \
                cur_node->crossed_word[1], \
                words[cur_node->crossed_word[1]].word);
        printf("Letters:\t%d\t%d\n", \
                cur_node->crossed_word_letter[0], \
                cur_node->crossed_word_letter[1]);
        printf("Orient:\t\t%s\t%s\n", \
                cur_node->word_orient[0] == 1 ? "hor" : "ver", \
                cur_node->word_orient[1] == 1 ? "hor" : "ver" );
        printf("Word coords:\t[%d, %d]\t[%d, %d]\n\n", \
                cur_node->word_coord[0][0], \
                cur_node->word_coord[0][1], \
                cur_node->word_coord[1][0], \
                cur_node->word_coord[1][1]);

        // Mark intersection letters lowercase back
        for (i = 0; i < 2; i++)
        {
            index = cur_node->crossed_word_letter[i];
            words[cur_node->crossed_word[i]].word[index] = \
                tolower(words[cur_node->crossed_word[i]].word[index]);
        }

        cur_node = cur_node->parent;
    }
    printf("----------------------------------------------\n");
    return 0;
}

// Free the allocated memory
int clear_branch(struct strie_pair *node)
{
    struct strie_pair *cur_node = node;
    struct strie_pair *next_node = NULL;

    if (!node)
        return 0;

    while (cur_node)
    {
        while (cur_node->firstchild)
            cur_node = cur_node->firstchild;

        if (!(next_node = cur_node->brother))
        {
            next_node = cur_node->parent;
            // If we are not in the root children...
            if (next_node)
            {
                // ...set parent's firstchild to NULL not to process
                // already freed child
                next_node->firstchild = NULL;
            }
        }

        if (cur_node->available_first_children)
            free(cur_node->available_first_children);
        free(cur_node);

        cur_node = next_node;
    }

    return 0;
}

clock_t begin_timer()
{
    clock_t start_time = clock();
    return start_time;
}

float stop_timer(clock_t start_time)
{
    clock_t end_time = clock();
    float delta = (float)(end_time - start_time) / CLOCKS_PER_SEC;
    return delta;
}

int main(int argc, char **argv)
{
    int i, j, wordnum = 0;
    int limit = 0;
    FILE *fwords = NULL;

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        fprintf(stderr, "Couldn't catch SIGINT\n");

    printf("Welcome to Crossword Generator v0.1\n");
    printf("===================================\n");

    if (2 > argc)
    {
        printf("Usage: %s <file with a list of words> [number of crossings to stop]\n", argv[0]);
        printf("\nMax words: %d\nMax wordlen: %d\n", MAXWORDS, MAXWORDLEN);
        return 1;
    }

    // Read input words to the words[] array
    if (NULL == (fwords = fopen(argv[1], "r")))
    {
        fprintf(stderr, "Can't open the file \"%s\"!\n", argv[1]);
        return 1;
    }
    for (i = 0; NULL != fgets(words[i].word, MAXWORDLEN, fwords) && i < MAXWORDS; i++)
    {
        // Remove newline symbol
        words[i].wordlen = strlen(words[i].word) - 1;
        words[i].word[words[i].wordlen] = '\0';
        for (j = 0; j < words[i].wordlen; j++)
            words[i].word[j] = tolower(words[i].word[j]);
        printf("Word #%d: %s, %d\n", i, words[i].word, words[i].wordlen);
    }
    wordnum = i;

    // Set limit
    if (2 < argc)
    {
        limit = atoi(argv[2]);
        if (1 == limit)
            limit--;
        limit--;
    }

    if (limit > 0)
    {
        printf("\nCrossword generation will be stopped when the one is created with at least %d crossings.\n", limit + 1);
    }
    else if (limit < 0)
    {
        printf("\nCrossword generation will be stopped when any finished chain of words is found.\n");
    }

    // Build inital word pairs that we'll be using a lot later
    if (build_pairs(wordnum))
    {
        fprintf(stderr, "Error building pairs of words.\n");
        return 1;
    }

    // Fill the tree with all possible pairs.
    // In fact usually it's enough to scan only 1/3 of the total number of
    // words.
    for (i = 0; i < wordnum; i++)
    {
        int ret;
        if (BUILD_FINISHED == (ret = build_branch(wordnum, words[i].firstchild, limit)))
            break;
    }

#ifdef DEBUG
    for (i = 0; i < wordnum; i++)
    {
        printf("\n--------------------------------\nword[%d]=%s\n--------------------------------\n", i, words[i].word);
        print_strie(words[i].firstchild);
    }
#endif

    // Print the best branch if any
    print_branch(best_branch);

    // Free the memory
    for (i = 0; i < wordnum; i++)
        clear_branch(words[i].firstchild);

    return 0;
}
