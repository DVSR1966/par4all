int main() {
  int i;

#pragma toto
  {
    i=0;
#pragma tata
    { 
      int i;
      i=0;
      i++;
    }
#pragma X
    i=0;
  }
#pragma X
  i=0;
  
  return i;
}
