int main()
{
  int a[1], b[1], c[1];

  a[0] = 0;
  b[0] = 1;
  {
    int d[1] = {1};
    c[0] = a[0] + d[0];
    a[0]= 2;
  }
  return b[0];
}
