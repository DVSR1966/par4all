//Generated by c2fsm -s 
model wcet1 {
var a,i,j,k;
//@parameters i;
states stop,start,lbl_6,lbl_9;
transition t_24 :={
  from  := start;
  to    := lbl_6;
  guard := true;
  action:= j' = 0, k' = 0;
};
transition t_15 :={
  from  := lbl_6;
  to    := stop;
  guard := ( (3 <= i) || (10 <= j) );
  action:=;
};
transition t_16 :={
  from  := lbl_6;
  to    := lbl_9;
  guard := ( (i <= 2) && (j <= 9) );
  action:=;
};
transition t_19 :={
  from  := lbl_9;
  to    := lbl_6;
  guard := true;
  action:= j' = j+1;
};
transition t_22 :={
  from  := lbl_9;
  to    := lbl_6;
  guard := true;
  action:= a' = a+2, j' = j+1, k' = k+1;
};
}
strategy dumb {
    Region init := { state = start && ( ( (0 <= i) && (0 <= a) ) && (a <= 5)
 ) };
}

