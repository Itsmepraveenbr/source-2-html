
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "s2html_event.h"

#define SIZE_OF_SYMBOLS (sizeof(symbols))
#define SIZE_OF_OPERATORS (sizeof(operators))
#define WORD_BUFF_SIZE	100

/********** Internal states and event of parser **********/
typedef enum
{
	PSTATE_IDLE,
	PSTATE_PREPROCESSOR_DIRECTIVE,
	PSTATE_SUB_PREPROCESSOR_MAIN,
	PSTATE_SUB_PREPROCESSOR_RESERVE_KEYWORD,
	PSTATE_SUB_PREPROCESSOR_ASCII_CHAR,
	PSTATE_HEADER_FILE,
	PSTATE_RESERVE_KEYWORD,
	PSTATE_NUMERIC_CONSTANT,
	PSTATE_STRING,
	PSTATE_SINGLE_LINE_COMMENT,
	PSTATE_MULTI_LINE_COMMENT,
	PSTATE_ASCII_CHAR
}pstate_e;

/********** global variables **********/

/* parser state variable */
static pstate_e state = PSTATE_IDLE;

/* sub state is used only in preprocessor state */
static pstate_e state_sub = PSTATE_SUB_PREPROCESSOR_MAIN;

/* event variable to store event and related properties */
static pevent_t pevent_data;  //structure to store the data
static int event_data_idx = 0;  //indexing variable

static char word[WORD_BUFF_SIZE];   //buffer to store words
static int word_idx = 0;          //indexing variable


static char* res_kwords_data[] = {"const", "volatile", "extern", "auto", "register",
   						   "static", "signed", "unsigned", "short", "long", 
						   "double", "char", "int", "float", "struct", 
						   "union", "enum", "void", "typedef", ""
						  };

static char* res_kwords_non_data[] = {"goto", "return", "continue", "break", 
							   "if", "else", "for", "while", "do", 
							   "switch", "case", "default","sizeof", ""
							  };

static char operators[] = {'/', '+', '*', '-', '%', '=', '<', '>', '~', '&', ',', '!', '^', '|'};
static char symbols[] = {'(', ')', '{', '[', ':'};

/********** state handlers **********/
pevent_t * pstate_idle_handler(FILE *fd, int ch);
pevent_t * pstate_single_line_comment_handler(FILE *fd, int ch);
pevent_t * pstate_multi_line_comment_handler(FILE *fd, int ch);
pevent_t * pstate_numeric_constant_handler(FILE *fd, int ch);
pevent_t * pstate_string_handler(FILE *fd, int ch);
pevent_t * pstate_header_file_handler(FILE *fd, int ch);
pevent_t * pstate_ascii_char_handler(FILE *fd, int ch);
pevent_t * pstate_reserve_keyword_handler(FILE *fd, int ch);
pevent_t * pstate_preprocessor_directive_handler(FILE *fd, int ch);
pevent_t * pstate_sub_preprocessor_main_handler(FILE *fd, int ch);

/********** Utility functions **********/

/* function to check if given word is reserved key word */
static int is_reserved_keyword(char *word)
{
	int idx = 0;

	/* search for data type reserved keyword */
	while(*res_kwords_data[idx])
	{
		if(strcmp(res_kwords_data[idx++], word) == 0)
			return RES_KEYWORD_DATA;
	}

	idx = 0; // reset index

	/* search for non data type reserved key word */
	while(*res_kwords_non_data[idx])
	{
		if(strcmp(res_kwords_non_data[idx++], word) == 0)
			return RES_KEYWORD_NON_DATA;
	}

	return 0; // word did not match, return false
}

/* function to check symbols */
static int is_symbol(char c)
{
	int idx;
	for(idx = 0; idx < SIZE_OF_SYMBOLS; idx++)
	{
		if(symbols[idx] == c)
			return 1;
	}

	return 0;
}

/* function to check operator */
static int is_operator(char c)
{
	int idx;
	for(idx = 0; idx < SIZE_OF_OPERATORS; idx++)
	{
		if(operators[idx] == c)
			return 1;
	}

	return 0;
}

/* to set parser event */
static void set_parser_event(pstate_e s, pevent_e e)
{
	pevent_data.data[event_data_idx] = '\0';
	pevent_data.length = event_data_idx;
	event_data_idx = 0;
	state = s;
	pevent_data.type = e;
    
}


/************ Event functions **********/

/* This function parses the source file and generate 
 * event based on parsed characters and string
 */
pevent_t *get_parser_event(FILE *fd)
{
	int ch, pre_ch;    //variable to store the present and previous character
	pevent_t *evptr = NULL;       //structure pointer

	/* Read char by char */
	while((ch = fgetc(fd)) != EOF)
	{
#ifdef DEBUG
	//	putchar(ch);
#endif
        //to check the types of event obtained
		switch(state)
		{
			case PSTATE_IDLE :
				if((evptr = pstate_idle_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_SINGLE_LINE_COMMENT :
				if((evptr = pstate_single_line_comment_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_MULTI_LINE_COMMENT :
				if((evptr = pstate_multi_line_comment_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_PREPROCESSOR_DIRECTIVE :
				if((evptr = pstate_preprocessor_directive_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_RESERVE_KEYWORD :
				if((evptr = pstate_reserve_keyword_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_NUMERIC_CONSTANT :
				if((evptr = pstate_numeric_constant_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_STRING :
				if((evptr = pstate_string_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_HEADER_FILE :
				if((evptr = pstate_header_file_handler(fd, ch)) != NULL)
					return evptr;
				break;

			case PSTATE_ASCII_CHAR :
				if((evptr = pstate_ascii_char_handler(fd, ch)) != NULL)
					return evptr;
				break;

			default : 
				printf("unknown state\n");
				state = PSTATE_IDLE;
				break;
		}
	}

	/* end of file is reached, move back to idle state and set EOF event */
	set_parser_event(PSTATE_IDLE, PEVENT_EOF);

	return &pevent_data; // return final event
}


/********** IDLE state Handler **********
 * Idle state handler identifies
 ****************************************/
pevent_t * pstate_idle_handler(FILE *fd, int ch)
{
	int pre_ch;   //variable to hold the previous character

    //to check for the type of character obtained
	switch(ch)
	{
		case '\'' : // begining of ASCII char 
            state = PSTATE_ASCII_CHAR;   //change the state as ASCII char
            pevent_data.data[event_data_idx++] = ch;  //add the ascii char to the array
			break;

		case '/' :
			pre_ch = ch;
			if((ch = fgetc(fd)) == '*') // multi line comment
			{
				if(event_data_idx) // we have regular exp in buffer first process that
				{
					fseek(fd, -2L, SEEK_CUR); // unget chars
					set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
					return &pevent_data;
				}
				else //	multi line comment begin 
				{
#ifdef DEBUG	
					printf("Multi line comment Begin : /*\n");
#endif
					state = PSTATE_MULTI_LINE_COMMENT;    //change the state as multi line comment
                    //add characters to the array
					pevent_data.data[event_data_idx++] = pre_ch;   
					pevent_data.data[event_data_idx++] = ch;
				}
			}
			else if(ch == '/') // single line comment
			{
				if(event_data_idx) // we have regular exp in buffer first process that
				{
					fseek(fd, -2L, SEEK_CUR); // unget chars
					set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
					return &pevent_data;
				}
				else //	single line comment begin
				{
#ifdef DEBUG	
					printf("Single line comment Begin : //\n");
#endif
					state = PSTATE_SINGLE_LINE_COMMENT;   //change the state to single line comment
                    // add // to the array
					pevent_data.data[event_data_idx++] = pre_ch;
					pevent_data.data[event_data_idx++] = ch;
				}
			}
			else // it is regular exp
			{
				pevent_data.data[event_data_idx++] = pre_ch;
				pevent_data.data[event_data_idx++] = ch;
			}
			break;

		case '#' : //to detect preprocessor directive and macros
                if(event_data_idx) // we have regular exp in buffer first process that
                 {
                     fseek(fd, -1L, SEEK_CUR); // unget chars
                     set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
                     return &pevent_data;
                 }
                else
                {
                    state = PSTATE_PREPROCESSOR_DIRECTIVE;  //change the state top preprocessor directive
                    pevent_data.data[event_data_idx++] = ch;  //add # to the array
                }
			break;

		case '\"' : //to detect strings
               
            state = PSTATE_STRING;  //change the state to string
            pevent_data.data[event_data_idx++] = ch;
			break;
               

		case '0' ... '9' : // detect numeric constant            
             state = PSTATE_NUMERIC_CONSTANT;  //change the state as numeric conatant
		     pevent_data.data[event_data_idx++] = ch;	
			 break;
                
		case 'a' ... 'z' : // could be reserved key word                               
             state = PSTATE_RESERVE_KEYWORD;    //change the state as reserved keyword
		     pevent_data.data[event_data_idx++] = ch;
			 break;
                
		default : // Assuming common text starts by default.
            //if the character is a symbol,operator,whitespace,newline or tab makeing it as regular expression and printing into the html file
            if( (is_symbol(ch)) || (is_operator(ch)) || (ch == '\n') || (ch == ' ') || (ch == '\t'))     
            {
                pevent_data.data[event_data_idx++] = ch;  //add the character to array
                set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);  //call the set parser function as event regular expression
                return &pevent_data;   //returning the structure holding info
            }
            //else add to the character to the array 
            else
            {
			    pevent_data.data[event_data_idx++] = ch;
			    break;
            }
	}

	return NULL;
}

//to handle preprocessor statements
pevent_t * pstate_preprocessor_directive_handler(FILE *fd, int ch)
{
	int tch;
    //checking the subtype of the preprocessor statements
	switch(state_sub)
	{
		case PSTATE_SUB_PREPROCESSOR_MAIN :
			return pstate_sub_preprocessor_main_handler(fd, ch);

		case PSTATE_SUB_PREPROCESSOR_RESERVE_KEYWORD :
			return pstate_reserve_keyword_handler(fd, ch);

		case PSTATE_SUB_PREPROCESSOR_ASCII_CHAR :
			return pstate_ascii_char_handler(fd, ch);

		default :
				printf("unknown state\n");
				state = PSTATE_IDLE;
	}

	return NULL;
}

//to handle preprocessor statements of sub type main_handler
pevent_t * pstate_sub_preprocessor_main_handler(FILE *fd, int ch)
{
	/* write a switch case here to detect several events here
	 * This state is similar to Idle state with slight difference
	 * in state transition.
	 * return event data at the end of event
	 * else return NULL
	 */
    
    //to check for the type of character to decide what type of preprocessor it is
    switch (ch)
	{
	    case '<': // Begin of standard header file
		    state = PSTATE_HEADER_FILE;   //change the state to header file
            pevent_data.property = STD_HEADER_FILE;  //change the sub state as std header file
		    //pevent_data.data[event_data_idx++] = ' ';
		    break;

        case '"' :  //begin of user header file
            state = PSTATE_HEADER_FILE;    //change the state to header file
            pevent_data.property = USER_HEADER_FILE;  //change the sub state as user header file
            pevent_data.data[event_data_idx++] = ch;   // add the character to array
            break;

	    default :   //if the type is not of header file collect the next character and checking for keywords by setting state sub as RESERVE_KEYWORD
		    pevent_data.data[event_data_idx++] = ch;   
            state_sub = PSTATE_SUB_PREPROCESSOR_RESERVE_KEYWORD;
            break;
    }
        
   return NULL;
}

//to handle the header file
pevent_t * pstate_header_file_handler(FILE *fd, int ch)
{
	/* write a switch case here to store header file name
	 * return event data at the end of event
	 * else return NULL
	 */
   // to check for the ending character 
   switch (ch)
	{
	case '>': // End of standard header file
		//pevent_data.data[event_data_idx++] = ' ';
		set_parser_event(PSTATE_IDLE, PEVENT_HEADER_FILE);  //call the set parser event function and add the obtained statements to a structure
		return &pevent_data;  //returning the structure address

    case '"' :  // end of user header file
        pevent_data.data[event_data_idx++] = ch;  // add the character to array
        set_parser_event(PSTATE_IDLE, PEVENT_HEADER_FILE); //call the set parser event function and add the obtained statements to     a structure
        return &pevent_data;  //returning the structure address

	default: // to collect the Characters within the header file
		pevent_data.data[event_data_idx++] = ch;
		break;
	}

	return NULL;
}

//to handle the reserve keyword the are present in the preprocessor statements and in the program
pevent_t * pstate_reserve_keyword_handler(FILE *fd, int ch)
{
     // * write a switch case here to store words
     // * return event data at the end of event
     // * else return NULL

    //to check if we are checking for words of preprocessor statement
    if (state == PEVENT_PREPROCESSOR_DIRECTIVE)
    {
        //to check for the ending of word starting with #
        switch (ch)
        {
            case ' ':                           // Space after the word
            case '\t':                         // Tab after the word
            case '\n':                         // Newline after the word
                
                pevent_data.data[event_data_idx++] = ch; //add the obtained char to the array

                //to check the next chars after the word and set the appropriate state
                while((ch = fgetc(fd) ))
                {
                    if(ch == ' ')   //to skip the whitespaces
                    {
                        pevent_data.data[event_data_idx++] = ch;
                        continue;
                    }
                    else if(ch == '<' || ch == '"')  //to check if it is a header or not
                    {
                        fseek(fd, -1L ,SEEK_CUR);  //to put back the char to the stream
                        pevent_data.data[event_data_idx] = '\0';   // Null terminate the word
                        
                        //to call parser event function by making the state as preprocessor directive    
                        set_parser_event( PSTATE_PREPROCESSOR_DIRECTIVE, PEVENT_PREPROCESSOR_DIRECTIVE);
                        state_sub = PSTATE_SUB_PREPROCESSOR_MAIN; //make the sub state as preprocessor main
                        break;
                    }
                    else     //if it is a macro or any other
                    {
                        fseek(fd, -1L, SEEK_CUR);  //to put back the char to the stream

                        //to call parser event function by making the state as idle
                        set_parser_event(PSTATE_IDLE, PEVENT_PREPROCESSOR_DIRECTIVE);
                        state_sub =  PSTATE_SUB_PREPROCESSOR_MAIN; //make the sub state as preprocessor main
                        break;
                    }
                    //fseek(fd, -1, SEEK_CUR); // Unget the last character
                }
                    return &pevent_data;  //retirn the structure address
                 
            default: // Collect characters of the preprocessor
                pevent_data.data[event_data_idx++] = ch;
                break;
        }

    }
    //if the word does not belong to preprocessor
    else
    {
        int keyword_type;  //variable to hold type of keyword (data or non data)
        
        //if the word ends with symbols or operators then check the word 
        if(is_symbol(ch) || is_operator(ch))
        {
             pevent_data.data[event_data_idx] = '\0';   // Null terminate the word that was read before
      
             //check if the word is keyword or not
             keyword_type = is_reserved_keyword(pevent_data.data);
 
                 if(keyword_type) // Check if the word is reserved
                 {
                     //call the set function by makinf the evant as reserved keyword
                    set_parser_event(PSTATE_IDLE, PEVENT_RESERVE_KEYWORD);
                    pevent_data.property = keyword_type; //set the property of the keyword

                 }
                else // Regular expression, not reserved
                {
                    //call the set function by making the event as regular expression
                     set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
                 }

                 fseek(fd, -1, SEEK_CUR); // Unget the last character
                 return &pevent_data;
        }     
        else  // to do the same thing if the words are ending with sapce or newline
        {
            switch (ch)
	        {
	            case ' ':							 // Space after word
	            case '\t':							 // Tab after word
	            case '\n':							 // Newline after word 
                case ';':                           //end of statement after word    
                    pevent_data.data[event_data_idx] = '\0';   // Null terminate the word
		
                    //printf("checking-%s\n",pevent_data.data);

                    //check if the word is keyword or not
                    keyword_type = is_reserved_keyword(pevent_data.data);

                    if(keyword_type) // Check if the word is reserved
		            {
                        //call the set function by makinf the evant as reserved keyword
			            set_parser_event(PSTATE_IDLE, PEVENT_RESERVE_KEYWORD);
                        pevent_data.property = keyword_type;   //set the property of the keyword
                        //printf("%s is a keyword\n",pevent_data.data);
                   
		            }
		            else // Regular expression, not reserved
		            {
                        //call the set function by making the event as regular expression
			            set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
                        // printf("%s is a  reg_exp\n",pevent_data.data);
		            }
		
                    fseek(fd, -1, SEEK_CUR); // Unget the last character
		            return &pevent_data;   //return the structure address

	            default: // Collect characters of the reserved keyword
		            pevent_data.data[event_data_idx++] = ch;
		            break;
	        }
        }

    }
 
     return NULL;
}

//to handle the numeric constants
pevent_t * pstate_numeric_constant_handler(FILE *fd, int ch)
{
	/* write a switch case here to store digits
	 * return event data at the end of event
	 * else return NULL
	 */

    if (isdigit(ch)) // Check if character is a digit
	{
		pevent_data.data[event_data_idx++] = ch;
	}
	else // End of numeric constant
	{
		set_parser_event(PSTATE_IDLE, PEVENT_NUMERIC_CONSTANT);
		fseek(fd, -1, SEEK_CUR); // to move the offset one position back	
        return &pevent_data;
	}

	return NULL;

}

//to handle strings
pevent_t * pstate_string_handler(FILE *fd, int ch)
{
	/* write a switch case here to store string
	 * return event data at the end of event
	 * else return NULL
	 */
    
    switch (ch)
	    {
	    case '\"': // End of string
		    pevent_data.data[event_data_idx++] = ch; // adding the char to array
		    set_parser_event(PSTATE_IDLE, PEVENT_STRING);  //calling thge set function by making event as string
		    return &pevent_data;

	    case '\\': // Escape character in string
		    pevent_data.data[event_data_idx++] = ch;
		    ch = fgetc(fd);
		    if(ch != EOF) // Read the escaped character
		    {
			    pevent_data.data[event_data_idx++] = ch;
		    }
		    break;

	    default: // Regular string character
		    pevent_data.data[event_data_idx++] = ch;
		    break;
	}

	return NULL;
}

//to handle sinle line comments
pevent_t * pstate_single_line_comment_handler(FILE *fd, int ch)
{
	int pre_ch;
	switch(ch)
	{
		case '\n' : /* single line comment ends here */
#ifdef DEBUG	
			printf("\nSingle line comment end\n");
#endif
			pre_ch = ch;
			pevent_data.data[event_data_idx++] = ch;
			set_parser_event(PSTATE_IDLE, PEVENT_SINGLE_LINE_COMMENT);
			return &pevent_data;
		default :  // collect single line comment chars
			pevent_data.data[event_data_idx++] = ch;
			break;
	}

	return NULL;
}

//to handle multi line comments
pevent_t * pstate_multi_line_comment_handler(FILE *fd, int ch)
{
	int pre_ch;
	switch(ch)
	{
		case '*' : /* comment might end here */
			pre_ch = ch;
			pevent_data.data[event_data_idx++] = ch;
			if((ch = fgetc(fd)) == '/')
			{
#ifdef DEBUG	
				printf("\nMulti line comment End : */\n");
#endif
				pre_ch = ch;
				pevent_data.data[event_data_idx++] = ch;
				set_parser_event(PSTATE_IDLE, PEVENT_MULTI_LINE_COMMENT);
				return &pevent_data;
			}
			else // multi line comment string still continued
			{
				pevent_data.data[event_data_idx++] = ch;
			}
			break;
		case '/' :
			/* go back by two steps and read previous char */
			fseek(fd, -2L, SEEK_CUR); // move two steps back
			pre_ch = fgetc(fd); // read a char
			fgetc(fd); // to come back to current offset

			pevent_data.data[event_data_idx++] = ch;
			if(pre_ch == '*')
			{
				set_parser_event(PSTATE_IDLE, PEVENT_MULTI_LINE_COMMENT);
				return &pevent_data;
			}
			break;
		default :  // collect multi-line comment chars
			pevent_data.data[event_data_idx++] = ch;
			break;
	}

	return NULL;
}

//to handle ascii characters
pevent_t * pstate_ascii_char_handler(FILE *fd, int ch)
{
	/* write a switch case here to store ASCII chars
	 * return event data at the end of event
	 * else return NULL
	 */
    switch(ch)
    {

        case '\'' :  //end od ACSII character
        case ' ' :
            pevent_data.data[event_data_idx++] = ch;
            set_parser_event(PSTATE_IDLE, PEVENT_ASCII_CHAR);
            return &pevent_data;
            break;

        default :  //collect the character 
            pevent_data.data[event_data_idx++] = ch;
            break;
    }

    return NULL;
}
/**** End of file ****/
