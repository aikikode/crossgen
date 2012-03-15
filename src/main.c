/*
 * Crossword Generator main file
 *
 * Author: Denis Kovalev (aikikode@gmail.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXWORDS    1000  // maximum number of words to create a crossword from
#define MAXWORDLEN  30

struct schild {
    struct schild *nextchild;
    int   crossed_word2_num;
    int   crossed_word1_letter;
    int   crossed_word2_letter;
};

struct cross_elem {
    char  word[MAXWORDLEN];
    int   wordlen;
    int   childnum;       // number of pairs between this words and later words
    struct schild *firstchild;
};

struct cross_elem words[MAXWORDS];

int main(int argc, char **argv)
{
    int i, j, k, wordnum = 0;

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
    for (i = 0; i < wordnum - 1; i++)
    {
        for (j = i + 1; j < wordnum; j++)
        {
            // Find all crossings between words 'i' and 'j'
            for (k = 0; k < words[i].wordlen; k++)
            {
                char *tmp_wordpart = words[j].word;
                while (NULL != (tmp_wordpart = strchr(tmp_wordpart, words[i].word[k])))
                {
                    printf("crossing between %s and %s: %c, %d, %d\n", words[i].word, words[j].word, tmp_wordpart[0], k, tmp_wordpart - words[j].word);
                    tmp_wordpart++;
                }
            }
        }
    }
    return 0;
}
