/* This code is standard conformant because struct s is
   redeclared in a different scope */

void struct08()
{
  int i;
  struct s
  {
    int l;
  };

  if(i>0) {
    union
    {
      struct s
      {
	float x;
      } d;
      int i;
    } u;
  }
}
