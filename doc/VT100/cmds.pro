
test :- dict(X,_), check(X), fail; true.

check([_,head]) :- !. /* TITLE             */
check([_,emus]) :- !. /* List Word         */
check([_,dflt]) :- !. /* List NumberOrWord */
check([_,sect]) :- !. /* List Word // Dotted Name */
check([_,code]) :- !. /* [ TYP, ("Char"/none/Numb), [arg, ...] ] */
check([_,text]) :- !. /* [ String/nl/ref(Sym) ... ] */

check([_,table,'XPS']) :- !. /* interpretation */
check([_,table,'XEX']) :- !. /* interpretation */
check([_,table,'Ps']) :- !.  /* interpretation */

/*check([_,table,_]) :- !, print(X), nl.*/

check(X) :- !, print(X), nl.

/* ----------- */

/* 
   Codes
   - ESC <Char>
   - HSH <Char>
   - CTL <Hex>
*/

head(Name,Title)       :- dict([Name,head],Title).
emus(Name,Emus)        :- dict([Name,emus],Emus).
dflt(Name,Defaults)    :- dict([Name,dflt],Defaults).
sect(Name,DottedSect)  :- dict([Name,sect],DottedSect).

code(Name,Code)        :- dict([Name,code],Desc), tcode(Desc,Code).

tcode(['PRN',none,[]],prn) :- !.
tcode(['DEL',none,[]],ctl(127)) :- !.
tcode(['CTL',Num,[]],ctl(Num)) :- !.
tcode(['ESC',Chr,[]],esc(Chr)) :- !.
tcode(['HSH',Chr,[]],esc(Chr)) :- !.
tcode(['CSI',Chr,[P,'...']],csi(Chr,list(P))) :- !.
tcode(['CSI',Chr,Parm],csi(Chr,Parm)) :- !.
tcode(['PRI',Chr,[P,'...']],pri(Chr,list(P))) :- !.
tcode(['PRI',Chr,Parm],pri(Chr,Parm)) :- !.
tcode(['SCS',none,[A,B]],scs([A,B])) :- !.
tcode(['VT5',none,[X,Y]],vt5([X,Y])) :- !.
tcode(P,P) :- writef("\n - fail\n %t \n\n",[P]).

pheads :-
  head(N,T),
  writef("%w - %s\n",[N,T]),
  fail; true.

pcodes :-
  code(N,P),
  writef("%w - %t\n",[N,P]),
  fail; true.

