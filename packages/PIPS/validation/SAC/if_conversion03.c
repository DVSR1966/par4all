void foo()
{
  int i,tab[4][4];
  for(i=0;i<4;i++)
    if(tab[i][i]>i)
      {
	int j=i*2;
	tab[i][i]=i;
	tab[i][i]-=j;
      }

}
