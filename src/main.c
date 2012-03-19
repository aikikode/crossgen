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

#define DEBUG       1

// The elements structures are at initial state for now. Most probably they'll
// change in the future.

struct strie_pair {
    int   crossed_word1_num;
    int   crossed_word2_num;
    int   crossed_word1_letter;
    int   crossed_word2_letter;
    int   childnum;       // number of all children of the current node
    int   word1_orient;   // 0 - horisontal; 1 - vertical
    int   word2_orient;
    int   word1_coord[2]; // coordinates of the beginning of the word
    int   word2_coord[2];
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

int fill_strie(struct strie_pair *node);

/*
 * Take wordnum elements and find all crossings between them
 */
int build_pairs(int wordnum)
{
    int i, j, k;
    for (i = 0; i < wordnum - 1; i++)
    {
        for (j = i + 1; j < wordnum; j++)
        {
            // When we add new pair we should know what the latest one was to
            // add just after it (add its brother)
            struct strie_pair *latest_child = NULL;
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
                    if (NULL == (schild = malloc(sizeof(struct strie_pair))))
                    {
                        fprintf(stderr, "Not enough memory!\n");
                        return 1;
                    }
                    schild->crossed_word1_num    = i;
                    schild->crossed_word1_num    = j;
                    schild->crossed_word1_letter = k;
                    schild->crossed_word2_letter = tmp_wordpart - words[j].word;
                    schild->word1_orient = 0;
                    schild->word2_orient = 1;
                    schild->word1_coord[0] = 0;
                    schild->word1_coord[1] = 0;
                    schild->word2_coord[0] = schild->crossed_word1_letter - 1;
                    schild->word2_coord[1] = schild->crossed_word2_letter - 1;
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

// Scan the found pairs and continue filling the strie with pairs combinations.
// Recursive function.
int fill_strie(struct strie_pair *node)
{
    struct strie_pair *cur_node = node;
    /* First have a look at current pair and try to add its members as children
     * (find other pairs of its members).
     *
     * Locate first word pair
     */

    if (cur_node->brother)
    {
        fill_strie(cur_node->brother);
    }
    else if (cur_node->parent)
    {
        fill_strie(cur_node->parent);
    }
    // This code is reached only when all pairs for the word have been
    // processed
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
        fill_strie(words[i].firstchild);



    return 0;
}
