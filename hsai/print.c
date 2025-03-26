

extern int put_char_sync( char c );

int printf(char c)
{
    c+=26;
    put_char_sync(c);
    return 0;
}