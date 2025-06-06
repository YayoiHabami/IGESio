# Entity Parameter Count for PD Section

<!-- omit in toc -->
## TOC

- [Fields in Parameters Data Section](#fields-in-parameters-data-section)
- [Directory Entry Parameters](#directory-entry-parameters)
- [Types of Physically Dependent Child Entities](#types-of-physically-dependent-child-entities)

## Fields in Parameters Data Section

Let $p(n)$ be the $n$-th parameter in the Parameter Data section of the entity. Note that the first parameter field is always the entity type number, and the number of parameters shown below excludes this field.

For example, since entity type 100 has 7 parameters, the line `"100,10.,-10.,25.,5.,25.-10.40.;"` in the Parameter Data section is assumed to have the following seven parameters:

$$p(1) = 10, p(2) = -10, p(3) = 25, p(4) = 5, p(5) = 25, p(6) = -10, p(7) = 40.$$

The following child elements are for "physically dependent" child elements. Here, the physical dependence of entity A on entity B means that entity A is referenced by the parameters of the PD section of entity B (excluding the additional pointers). In this case, this entity cannot exist without its parent, and the transformation matrix of the parent applies to its children.

In the Position of Child Elements column, $3^0$ indicates that the position has a pointer to the DE section if its value is non-zero positive, and $3^-$ indicates that its absolute value has a pointer to the DE section if its value is negative. Any other number, such as $3$, simply indicates that the number at that position is a pointer to the DE section.

- $p(3^0)$ is a pointer to the $p(3)$ element of the DE section if $p(3) > 0$.
  - $p(3^\varnothing)$ is a pointer to the $p(3)$ element of the DE section if $p(3)$ is not null.
- $p(3^-)$ is a pointer to the $|p(3)|$ element of the DE section if $p(3) < 0$.
- $p(3)$ is always a pointer to the $p(3)$ element of the DE section.

Also, a statement such as $a \;\text{to}\; b$ in the Position of Child Elements indicates that every number from $a$ to $b$ is a pointer to the DE section. Variables may also be used to define ranges, e.g., $5i \;\text{to}\; 5i+2\; (i=1\ldots N)$ is the range of all $2i$ to $2i+2$ from $i=1$ to $N$ (i.e., $5,6,7,\; 10,11,12,\; \ldots,\; 5N,5N+1,5N+2$).

|Type| Form| Size Specifier | Number of Parameters | Position of Child Elements |
|:--:|:---:|----------------|---|---|
| 0  | -   | - | $0$ | - |
| 100| 0   | - | $7$ | - |
| 102| 0   | $N = p(1)$ | $1+N$ | $2 \;\text{to}\; N+1$ |
| 104| 1~3 | - | $11$ | - |
| 106| 1, 11, 20~63| $N = p(2)$ | $3+2N$<br> | - |
| ^  | 2, 12| ^ | $2+3N$ | - |
| ^  | 3, 13| ^ | $2+6N$ | - |
| 108| 0   | - | $9$ | - |
| ^  | -1, 1| - | ^   | $5$ |
| 110| 0~2 | - | $6$ | - |
| 112| 0   | $N = p(4)$ | $17+13N$ | - |
| 114| 0   | $M = p(3)$<br>$N = p(4)$ | $6+M+N + 48(M+1)(N+1)$ | - |
| 116| 0   | - | $4$ | $4^0$ |
| 118| 0~1 | - | $4$ | $1$, $2$ |
| 120| 0   | - | $4$ | $1$, $2$ |
| 122| 0   | - | $4$ | $1$ |
| 123| 0   | - | $3$ | - |
| 124| 0~1, 10~12| - | $12$ | - |
| 125| 0~4 | - | $6$ | $6^0$ |
| 126| 0~6 | $K = p(1)$<br>$M = p(2)$ | $17+5K+M$ | - |
| 128| 0~9 | $K_1 = p(1)$<br>$K_2 = p(2)$<br>$M_1 = p(3)$<br>$M_2 = p(4)$ | $18 + 2(K_1 + K_2) + K_1K_2 + M_1 + M_2$ | - |
| 130| 0   | - | $14$ | $1$, $3^0$ |
| 132| 0   | - | $14$ | $4^\varnothing$, $8^\varnothing$, $10^\varnothing$, $14^\varnothing$ |
| 134| 0   | - | $4$ | $4^0$ |
| 136| 0   | $N = p(2)$ | $3+N$ | $3 \;\text{to}\; 2+N$ |
| 138| 0   | $N_C = p(1)$<br>$N_N = p(2+N_C)$ | $2+N_C+N_N(2+6N_C)$ | $2 \;\text{to}\; 1+N_C$, $3+N_C+(i-1)(2+6N_C)\; (i=1\ldots N_N)$ |
| 140| 0   | - | $5$ | $5$ |
| 141| 0   | $N = p(4)$<br>$K(i) = p(7+3(i-1)+\sum_{j=1}^{i-1}K(j))$ | $4+3N+\sum_{i=1}^{N}K(i)$ | $3$, $5+3(i-1)+\sum_{j=1}^{i-1}K(j)\; (i=1\ldots N)$, $8+3(i-1)+\sum_{j=1}^{i-1}K(j) \;\text{to}\; 4+3i+\sum_{j=1}^{i}K(j)\; (i=1\ldots N)$ |
| 142| 0   | - | $5$ | $2 \;\text{to}\; 4$ |
| 143| 0   | $N = p(3)$ | $3+N$ | $4 \;\text{to}\; 3+N$ (Type 141) |
| 144| 0   | $N_2 = p(3)$ | $4+N_2$ | $1$, $4^0$, $5 \;\text{to}\; 4+N_2$ |
| 146| 0~34| $N_V = p(4)$<br>$N_N = p(5)$ | $5 + N_N(N_V+2)$ | $1$, $7+(i-1)(N_V+2)\; (i=1\ldots N_N)$ |
| 148| 0~34| $N_V = p(4)$<br>$N_E = p(6)$<br>$N_L(i) = p(10+\sum_{h=1}^{i-1}(7+(N_VN_L(h)+1)N_{RL}(h)))\; (i=1\ldots N_E)$<br>$N_{RL}(i) = p(12+\sum_{h=1}^{i-1}(7+(N_VN_L(h)+1)N_{RL}(h)))\; (i=1\ldots N_E)$ | $6 + 7N_E + \sum_{i=1}^{N_E}(7+(N_VN_L(i)+1)N_{RL}(i))$ | $1$, $8+\sum_{h=1}^{i-1}(7+(N_VN_L(h)+1)N_{RL}(h))\; (i=1\ldots N_E)$ |
| 150| 0   | - | $12$ | - |
| 152| 0   | - | $13$ | - |
| 154| 0   | - | $8$ | - |
| 156| 0   | - | $9$ | - |
| 158| 0   | - | $4$ | - |
| 160| 0   | - | $8$ | - |
| 162| 0~1 | - | $8$ | $1$ |
| 164| 0   | - | $5$ | $1$ |
| 168| 0   | - | $12$ | - |
| 180| 0~1 | $N = p(1)$ | $N+1$ | $2^- \;\text{to}\; N^-$ |
| 182| 0   | - | $4$ | $1$ |
| 184| 0~1 | $N = p(1)$ | $1+2N$ | $2 \;\text{to}\; 1+2N$ |
| 186| 0   | $N = p(3)$ | $3+2N$ | $1$, $4+2(i-1)\; (i=1\ldots N)$ |
| 190| 0   | - | $2$ | $1$, $2$ |
| ^  | 1   | - | $3$ | $1 \;\text{to}\; 3$ |
| 192| 0   | - | $3$ | $1$, $2$ |
| ^  | 1   | - | $4$ | $1$, $2$, $4$ |
| 194| 0   | - | $4$ | $1$, $2$ |
| ^  | 1   | - | $5$ | $1$, $2$, $5$ |
| 196| 0   | - | $2$ | $1$ |
| ^  | 1   | - | $4$ | $1$, $3$, $4$ |
| 198| 0   | - | $4$ | $1$, $2$ |
| ^  | 1   | - | $5$ | $1$, $2$, $5$ |
| 202| 0   | - | $8$ | $1$, $2^0$, $3^0$, $7$, $8$ |
| 204| 0   | - | $7$ | $1$, $2$, $3^0$, $4$, $5$, $6^0$, $7^0$ |
| 206| 0   | - | $5$ | $1$, $2$, $3^0$ |
| 208| 0   | $N = p(6)$ | $6+N$ | $5$, $7 \;\text{to}\; 6+N$ |
| 210| 0   | $N = p(2)$ | $2+N$ | $1$, $3 \;\text{to}\; 2+N$ |
| 212| 0~8, 100~102, 105| $N_S = p(1)$ | $1+12N_S$ | $(5+12(i-1))^-\; (i=1\ldots N_S)$ |
| 213| 0   | $N_S = p(12)$ | $20N_S + 12$ | $(24+20(i-1))^-\; (i=1\ldots N_S)$ |
| 214| 1~12| $N = p(1)$ | $6+2N$ | - |
| 216| 0~2 | - | $5$ | $1 \;\text{to}\; 3$, $4^0$, $5^0$ |
| 218| 0   | - | $2$ | $1$, $2$ |
| ^  | 1   | - | $3$ | $1 \;\text{to}\; 3$ |
| 220| 0   | - | $3$ | $1$, $2$, $3^0$ |
| 222| 0   | - | $4$ | $1$, $2$ |
| ^  | 1   | - | $5$ | $1$, $2$, $5^0$ |
| 228| 0~3, 5001~9999| $N = p(2)$<br>$L = p(3+N)$ | $3+L+N$ | $3 \;\text{to}\; 2+N$, $4+N \;\text{to}\; 3+L+N$ |
| 230| 0~1 | $N = p(8)$ | $8+N$ | $1$, $9 \;\text{to}\; 8+N$ |
| 302| 5001~9999| $N_1 = p(4)$ | $4+N_1$
| 304| 1   | -  | $4$ | $2$ |
| ^  | 2   | $M = p(1)$ | $2+M$ | - |
| 306| 0   | TODO | TODO | - |
| 308| 0   | $N = p(3)$ | $3+N$ | $4 \;\text{to}\; 3+N$ |
| 310| 0   | $N = p(5)$<br>$N_M(i) = p(\sum_{j=1}^{i-1}N_M(j))$ | $5+4N+3\sum_{i=1}^{N}N_M(i)$ | $3^-$ |
| 312| 0~1 | - | $10$ | $3^-$ |
| 314| 0   | - | $4$ | - |
| 316| 0   | $N_P = p(1)$ | $1+3N_P$ | - |
| 320| 0   | $N_A = p(3)$<br>$N_C = p(7+N_A)$ | $7+N_C + N_A$ | $4 \;\text{to}\; 3+N_A$, $6+N_A$, $(8+N_A)^0 \;\text{to}\; (7+N_C + N_A)^0$ |
| 322| 0   | $N_A = p(3)$ | $3N_A+3$ | - |
| ^  | 1   | TODO | TODO | - |
| ^  | 2   |
| 402|
| 404| 0   | $N = p(1)$<br>$M = p(2+3N)$ | $2+M+3N$
| ^  | 1   | $N = p(1)$<br>$M = p(2+4N)$ | $2+M+4N$
| 406| 0~3, 5~36 | $N_P = p(1)$ | $1+N_P$ | - |
| 408| 0   | - | $5$ | $1$ |
| 410| 0   | - | $8$ | $3^0 \;\text{to}\; 8^0$ |
| ^  | 1   | - | $22$ | - |
| 412| 0   | $L_C = p(11)$ | $12+L_C$ | $1$ |
| 414| 0   | $L_C = p(9)$ | $10+L_C$ | $1$ |
| 416| 0, 2, 4 | - | $2$ | - |
| ^  | 1, 3 | - | $1$ | - |
| 418| 0   | $N_C = p(1)$ | $3+N_C$ | $3 \;\text{to}\; 3+N_C$ |
| 420| 0   | $N_C = p(11)$ | $11+N_C$ | $1$, $10^\varnothing$, $12^0 \;\text{to}\; (11+N_C)^0$ |
| 422| 0   | TODO | TODO | - |
| ^  | 1   | TODO | TODO | - |
| 430| 0~1 | - | $1$ | $1$ |
| 502| 1   | $N = p(1)$ | $1+3N$ | - |
| 504| 1   | $N = p(1)$ | $1+5N$ | $2+5(i-1)\; (i=1\ldots N)$, $3+5(i-1)\; (i=1\ldots N)$, $5+5(i-1)\; (i=1\ldots N)$ |
| 508| 0~1 | $N = p(1)$<br>$K(i) = p(6+5(i-1)+2\sum_{j=0}^{i-1}K(j))$ | $1+5N+2\sum{i=1}^{N}K(i)$ | $3+5(i-1)+2\sum_{j=0}^{i-1}K(j)\; (i=1\ldots N)$, $6+2k+5(i-1)+2\sum_{j=1}^{i-1}K(j)\; (i=1\ldots N,\; k=1\ldots K(i))$ |
| 510| 1   | $N = p(2)$ | $3+N$ | $1$, $4 \;\text{to}\; 3+N$ |
| 514| 1~2 | $N = p(1)$ | $1+2N$ | $2i\; (i=1\ldots N)$ |

> todo(size): 306, 322(1,2), 402, 422
> todo(pointer): 302, 322(2), 402, 404

## Directory Entry Parameters

In the Directory Entry section, each record contains 20 parameters (referred to as parameters 1 through 20). The possible values for each parameter are determined by the entity type and form number pair, which are summarized in the table below.

However, the following parameters are not included in the table:
- Parameter 2 (Parameter Data): Requires information from the Parameter Data section
- Parameters 10 and 20 (Sequence Numbers): Require information from other Directory Entry section records
- Parameters 16-19: No entity-specific specifications

The table below shows the allowable values for the remaining Directory Entry parameters by entity type and form number.

- Abbreviations used as column headers:
  - Type: Entity Type Number (DE parameter 1 and 11)
  - Forms: Form Number (DE parameter 15)
  - Class: Class for Entity (From Section 3.1 of IGES 5.3 standard)
  - 3: Structure (DE parameter 3)
  - 4: Line Font Pattern (DE parameter 4)
  - 5: Level (DE parameter 5)
  - 6: View (DE parameter 6)
  - 7: Transformation Matrix (DE parameter 7)
  - 8: Label Display (DE parameter 8)
  - 9: Status Number (DE parameter 9)
  - 12: Line Weight (DE parameter 12)
  - 13: Color Number (DE parameter 13)
  - 14: Parameter Line Count (DE parameter 14)
- The symbols used in the table:
  - `-`: `<n.a.>` (Not Applicable)
  - `#`: `#` (Integer)
  - `⇒`: `=>` (DE pointer (pointer is positive))
  - `⏩`: `#,=>` (Integer or DE pointer (pointer is negated))
  - `▶`: `0,=>` (zero or DE pointer (pointer is positive))
  - `‡`: Untested entity (See Section 1.9)
- Abbreviations used in the "Class" column ("Class of the Entity"):
  - C&S: Curve and Surface Geometry class
  - CSG: Constructive Solid Geometry class
  - BRp: Boundary Representation (B-Rep) Solid class
  - An.: Annotation class
  - St.: Structure class

|Type|Forms|Class| 3| 4| 5| 6| 7| 8| 9|12|13|14|
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 0 |  #  |St.|-|-|-|-|-|-|`********`|-|-|-|
|100|  0  |C&S|-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|102|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????????`|#|⏩|#|
|104| 1~3 | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|106| 1~3 | ^ |-|-|⏩|▶|▶|▶|`??????**`|#|⏩|#|
| ^ |11~13| ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
| ^ |20~21|An.|-|1|⏩|▶|▶|▶|`????01**`|#|⏩|#|
| ^ |31~38| ^ |-|1|⏩|▶|▶|▶|`????01**`|#|⏩|#|
| ^ |  40 | ^ |-|1|⏩|▶|▶|▶|`????01**`|#|⏩|#|
| ^ |  63 | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|108| -1~1|C&S|-|-|⏩|▶|▶|▶|`??????**`|-|⏩|#|
|110|  0  | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
| ^ | ‡1~2| ^ |-|⏩|⏩|▶|▶|▶|`????06**`|#|⏩|#|
|112|  0  | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|114|  0  | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|116|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????????`|#|⏩|#|
|118| 0~1 | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|120|  0  | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|122|  0  | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|‡123| 0  |<sup>[2]</sup>|-|-|-|-|-|-|`**0102**`|-|-|#|
|124|0~1, 10~12|C&S|-|-|-|-|▶|-|`****??**`|-|-|#|
|125| 0~4 | ^ |-|1|⏩|▶|▶|▶|`??????00`|#|⏩|#|
|126| 0~5 | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|128| 0~9 | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|130|  0  | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|132|  0  |St.|-|⏩|⏩|▶|▶|▶|`????04??`|#|⏩|#|
|134|  0  | ^ |-|-|-|-|⇒|-|`????04??`|-|⏩|#|
|136|  0  | ^ |-|⏩|-|-|-|▶|`********`|-|⏩|#|
|138|  0  | ^ |-|-|-|-|-|▶|`??????**`|-|-|#|
|140|  0  |C&S|-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|141|  0  | ^ |-|⏩|⏩|▶|▶|▶|`??????**`|#|⏩|#|
|142|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|143|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|144|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|‡146|0~34|St.|-|-|-|-|-|▶|`**??03**`|-|⏩|#|
|‡148|0~34| ^ |-|-|-|-|-|▶|`**??03**`|-|⏩|#|
|150|  0  |CSG|-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|152|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|154|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|156|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|158|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|160|  0  | ^ |-|⏩|⏩|▶|▶|▶|`00000000`|#|⏩|#|
|162|  0~1| ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|164|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|168|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????00**`|#|⏩|#|
|180|  0~1|CSG<sup>[1]</sup>|-|⏩|⏩|▶|▶|▶|`????00??`|#|⏩|#|
|‡182| 0  | ^ |-|-|⏩|▶|▶|▶|`**??03**`|-|⏩|#|
|184|  0~1| ^ |-|⏩|⏩|▶|▶|▶|`????02??`|#|⏩|#|
|‡186| 0  |BRp|-|-|⏩|-|▶|▶|`????????`|-|-|#|
|‡190| 0~1|C&S<sup>[2]</sup>|-|⏩|⏩|▶|▶|▶|`**????**`|#|⏩|#|
|‡192| 0~1| ^ |-|⏩|⏩|▶|▶|▶|`**01??**`|#|⏩|#|
|‡194| 0~1| ^ |-|⏩|⏩|▶|▶|▶|`**01??**`|#|⏩|#|
|‡196| 0~1| ^ |-|⏩|⏩|▶|▶|▶|`**01??**`|#|⏩|#|
|‡198| 0~1| ^ |-|⏩|⏩|▶|▶|▶|`**01??**`|#|⏩|#|
|202|  0  |An.|-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|‡204| 0  | ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|206|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|208|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|210|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|212|0~8, 100~102, 105| ^ |-|1|⏩|▶|▶|▶|`????01**`|#|⏩|#|
|‡213| 0  | ^ |-|1|⏩|▶|▶|▶|`????01**`|#|⏩|#|
|214| 1~12| ^ |-|⏩|⏩|▶|▶|▶|`????01**`|#|⏩|#|
|216|0, ‡1~2| ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|218|0, ‡1| ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|220|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|222|  0~1| ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|228|0~3, 5001~9999| ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|230|0, ‡1| ^ |-|⏩|⏩|▶|▶|▶|`????01??`|#|⏩|#|
|302|5001~9999|St.|-|-|-|-|-|-|`**0002**`|-|-|#|
|304|  1~2| ^ |-|1~5|-|-|▶|-|`**0002**`|-|-|#|
|‡306| 0  | ^ |-|-|-|-|-|-|`**0002**`|-|-|#|
|308|  0  | ^ |-|⏩|⏩|-|▶|▶|`**??02??`|#|⏩|#|
|310|  0  | ^ |-|-|-|-|-|-|`**0002**`|-|-|#|
|312|  0~1| ^ |-|-|⏩|▶|▶|▶|`??000200`|-|⏩|#|
|314|  0  | ^ |-|-|-|-|-|-|`**0002**`|-|1~8|#|
|‡316| 0  | ^ |-|-|-|-|-|-|`**0002**`|-|-|#|
|320|  0  | ^ |-|⏩|⏩|-|▶|▶|`**??02??`|#|⏩|#|
|322|  0~2| ^ |-|⏩|⏩|-|▶|▶|`**??02??`|#|⏩|#|
|402|1, 5, 7, 9, 12~15| ^ |-|-|-|-|-|-|`**????**`|-|-|#|
| ^ |3~4, ‡19| ^ |-|-|-|-|-|-|`**0001**`|-|-|#|
| ^ |  16 | ^ |-|-|-|-|-|-|`**??05**`|-|-|#|
| ^ |18, ‡20| ^ |-|-|-|-|-|-|`**??03**`|-|-|#|
| ^ |  21 | ^ |-|-|-|-|-|-|`**0102**`|-|-|#|
|404|  0~1| ^ |-|-|-|-|-|-|`**0001**`|-|-|#|
|406|1~2, 5~17, ‡18~25| ^ |-|-|⏩|-|-|-|`**??****`|-|-|#|
| ^ |  3  | ^ |-|-|⏩|-|-|-|`**00****`|-|-|#|
| ^ |  ‡26| ^ |-|-|⇒|-|-|-|`**??****`|-|-|#|
| ^ |‡27, ‡31| ^ |-|-|-|-|-|-|`**0102**`|-|-|#|
| ^ |‡28~30| ^ |-|-|-|-|-|-|`**0202**`|-|-|#|
| ^ |‡32~35| ^ |-|-|-|-|-|-|`**0101**`|-|-|#|
| ^ |  ‡36| ^ |-|-|-|-|-|-|`00010300`|-|-|#|
|408|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????????`|#|⏩|#|
|410|  0  | ^ |-|-|-|-|▶|-|`????01**`|-|-|#|
| ^ |  ‡1 | ^ |-|-|-|-|0|-|`????01**`|-|-|#|
|412|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????????`|#|⏩|#|
|414|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????????`|#|⏩|#|
|416|0~2, ‡3~4| ^ |-|-|-|-|-|-|`**????**`|-|-|#|
|418|  0  | ^ |-|-|-|▶|▶|▶|`??????**`|-|-|#|
|420|  0  | ^ |-|⏩|⏩|▶|▶|▶|`????????`|#|⏩|#|
|422|  0~1| ^ |⇒|-|-|-|-|-|`**????**`|-|-|#|
|430|  0~1|CSG<sup>[1]</sup>|-|⏩|⏩|▶|▶|▶|`????????`|#|⏩|#|
|‡502| 1  |BRp|-|-|⏩|-|-|▶|`??01??**`|-|-|#|
|‡504| 1  | ^ |-|-|⏩|-|-|▶|`??01??01`|-|-|#|
|‡508| 0~1| ^ |-|-|⏩|-|-|▶|`??01????`|-|-|#|
|‡510| 1  | ^ |-|-|⏩|-|-|▶|`??01????`|-|-|#|
|‡514| 1~2| ^ |-|-|⏩|-|-|▶|`????????`|-|-|#|

> **Note**: The symbol `<n.a.>` indicates that the field has no meaning for this entity. The field shall be empty or shall contain zero. Postprocessors shall ignore the value.
>
> In the Status Number field: The symbol (`**`) has the same meaning as `<n.a.>`. Preprocessors shall supply 00 in the field and postprocessors shall ignore the value (When the field is identified as `**`, the table may contain 00 for clarity). The symbol (`??`) means that an appropriate value from the defined range for this field shall appear.
>
> (From Section 2.2.4.4 of the IGES 5.3 standard)

> **Note [1]**: Used to merge primitive entities (CSG entities other than this) or B-Rep objects and combine them into more complex CSG entities.

> **Note [2]**: Also used as analytical surface entities for B-Rep solid models.

## Types of Physically Dependent Child Entities

In IGES 5.3, entities can reference (point to) other entities, creating dependency relationships. These relationships are primarily categorized as physical or logical (refer to IGES 5.3 Standard, Section 2.2.4.4.9.2). The table below defines these dependency types in the context of an entity P referencing an entity C.

| Dependency Type | Description |
|:---------------:|:------------|
| Independent | Entity P does not reference Entity C. |
| Physically Dependent | Entity C cannot exist without Entity P. Its position within the parent's definition space is determined by the parent's transformation matrix. |
| Logically Dependent | Entity C can exist independently. Its position is not affected by Entity P's transformation matrix. |
| Physically and Logically Dependent | Fulfills the criteria for both Physical and Logical Dependency. |

When Entity C is physically dependent on Entity P, it cannot exist independently. Therefore, identifying physical dependencies and retrieving an entity's physically dependent child elements is important. The table that follows (with columns such as "Name", "Type", "Implicit") summarizes for each entity type whether it has child elements and if those children are physically dependent.

- Abbreviations used as column headers:
  - Name: Name of the Entity (abbreviated)
  - Type: Entity Type
  - Forms: Forms of the Entity
  - Class: Class of the Entity
  - C2: Specifications for the class of the entity (See Section 3.1 to 3.6)
  - DE7: The 7th parameter (Transformation Matrix) of the Directory Entry section
  - Pointer: Define pointers with PD parameters
  - Implicit: Implicit case (Parent transformation matrix is applied to the child entity) (See Section 3.2.3, Table 4)
- The symbols used in the table:
  - `-`: `<n.a.>` (Not Applicable)
  - `#`: `#` (Integer)
  - `⇒`: `=>` (DE pointer (pointer is positive))
  - `▶`: `0,=>` (zero or DE pointer (pointer is positive))
  - `‡`: Untested entity (See Section 1.9)
- Abbreviations used in the "Class" column ("Class of the Entity"):
  - C&S: Curve and Surface Geometry class
  - CSG: Constructive Solid Geometry class
  - BRp: Boundary Representation (B-Rep) Solid class
  - An.: Annotation class
  - St.: Structure class

|Name          |Type|Forms|Class|C2|DE7|Pointer|Implicit<sup>[4][5]</sup>|
|:-------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
|Null          | 0 |  #  |St.| - |-| - | - |
|Circular Arc  |100|  0  |C&S|[6]|▶| ^ | ^ |
|Composite<br>Curve|102|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ✅<br>all constituents |
|Conic Arc     |104| 1~3 | ^ | ^ |▶| - | - |
|Copious Data  |106| 1~3 | ^ | - |▶| ^ | ^ |
|Linear Path   | ^ |11~12| ^ |[6]|▶| ^ | ^ |
| ^            | ^ |  13 | ^ | - |▶| ^ | ^ |
|Centerline    | ^ |20~21|An.| ^  |▶| ^ | ^ |
|Section       | ^ |31~38| ^ | ^ |▶| ^ | ^ |
|Witness Line  | ^ |  40 | ^ | ^ |▶| ^ | ^ |
|Planar Curve  | ^ |  63 |C&S|[6]|▶| ^ | ^ |
|Plane         |108|  0  | ^ | - |▶| ^ | ^ |
| ^            |108|-1, 1| ^ | ^ |▶| ✅<sup>[3a]</sup> | ✅<br>bounding curve |
|Line          |110|  0  |C&S|[6]|▶| - | - |
| ^            | ^ | ‡1~2| ^ | ^ |▶| ^ | ^ |
|Parametric<br>Spline Curve|112|  0  | ^ | ^ |▶| ^ | ^ |
|Parametric<br>Spline Surface|114|  0  | ^ | ^ |▶| ^ | ^ |
|Point         |116|  0  |C&S| - |▶| ✅<sup>[3b]</sup> | ✅<br>display symbol |
|Ruled Surface |118|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ✅<br>rail curves |
| ^            | ^ |  1  | ^ |[6]|▶| ^ | ^ |
|Surface of<br>Revolution|120|  0  | ^ | ^ |▶| ^ | ✅<br>axis, generatrix |
|Tabulated<br>Cylinder|122|  0  | ^ | ^ |▶| ^ | ✅<br>directrix |
|Direction     |‡123| 0  |C&S<sup>[7]</sup>|[2]|-| - | - |
|Transformation<br>Matrix |124|0~1, 10~12|C&S| - |▶| ^ | ^ |
|Flash         |125| 0~4 | ^ | ^ |▶| ✅<sup>[3b]</sup> | ✅<br>defining entity |
|Rational B<br>Spline Curve|126| 0~5 | ^ |[6]|▶| - | - |
|Rational B<br>Spline Surface|128| 0~9 | ^ | ^ |▶| ^ | ^ |
|Offset Curve  |130|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup><br>✅<sup>[3b]</sup> | ✅<br>base curve |
|Connect Point |132|  0  |St.| - |▶| ✅<sup>[3c]</sup> | ✅<br>display symbol<br>Text Display Templates |
|Node          |134|  0  | ^ | ^ |⇒| ✅<sup>[3b]</sup> | - |
|Finite Element|136|  0  | ^ | ^ |-| ✅<sup>[3a]</sup> | ^ |
|Nodal<br>Displacement<br>and Rotation|138|  0  | ^ | ^ |-| ^ | ✅<br>all General Notes<br>and Nodes |
|Offset Surface|140|  0  |C&S|[6]|▶| ^ | ✅<br>surface |
|Boundary      |141|  0  | ^ | - |▶| ^ | - |
|Curve on a<br>Parametric<br>Surface|142|  0  | ^ | ^ |▶| ^ | ^ |
|Bounded<br>Surface|143|  0  | ^ | ^ |▶| ^ | ^ |
|Trimmed<br>Surface|144|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup><br>✅<sup>[3b]</sup> | ✅<br>surface |
|Nodal Results |‡146|0~34|St.| ^ |-| ✅<sup>[3a]</sup> | - |
|Element Results|‡148|0~34| ^ | ^ |-| ^ | ^ |
|Block         |150|  0  |CSG| ^ |▶| - | ^ |
|Right Angular<br>Wedge|152|  0  | ^ | ^ |▶| ^ | ^ |
|Right Circular<br>Cylinder|154|  0  | ^ | ^ |▶| ^ | ^ |
|Right Circular<br>Cone|156|  0  | ^ | ^ |▶| ^ | ^ |
|Sphere        |158|  0  | ^ | ^ |▶| ^ | ^ |
|Torus         |160|  0  | ^ | ^ |▶| ^ | ^ |
|Solid of<br>Revolution|162|  0~1| ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|Solid of<br>Linear Extrusion|164|  0  | ^ | ^ |▶| ^ | ^ |
|Ellipsoid     |168|  0  | ^ | ^ |▶| - | ^ |
|Boolean Tree  |180|  0~1|CSG|[1]|▶| ✅<sup>[3d]</sup> | ^ |
|Selected<br>Component|‡182| 0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|Solid Assembly|184|  0~1| ^ | ^ |▶| ^ | ^ |
|Manifold Solid<br>B-Rep Object|‡186| 0  |BRp| - |▶| ^ | ^ |
|Plane Surface |‡190| 0~1|C&S|[2]|▶| ^ | ^ |
|Right Circular<br>Cylinder<br>Surface|‡192| 0~1| ^ |[2]<br>[6]|▶| ^ | ^ |
|Right Circular<br>Conical<br>Surface|‡194| 0~1| ^ | ^ |▶| ^ | ^ |
|Spherical<br>Surface|‡196| 0~1| ^ | ^ |▶| ^ | ^ |
|Toroidal<br>Surface|‡198| 0~1| ^ | ^ |▶| ^ | ^ |
|Angular<br>Dimension|202|  0  |An.| - |▶| ✅<sup>[3a]</sup><br>✅<sup>[3b]</sup> | ✅<br>all subordinate entities |
|Curve<br>Dimension|‡204| 0  | ^ | ^ |▶| ^ | - |
|Diameter<br>Dimension|206|  0  | ^ | ^ |▶| ^ | ✅<br>all subordinate entities |
|Flag Note     |208|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|General Label |210|  0  | ^ | ^ |▶| ^ | ^ |
|General Note  |212|0~8, 100~102, 105| ^ | ^ |▶| ✅<sup>[3d]</sup> | - |
|New<br>General Note|‡213| 0  | ^ | ^ |▶| ^ | ^ |
|Leader Arrow  |214| 1~12| ^ | ^ |▶| - | ^ |
|Linear<br>Dimension|216|0, ‡1~2| ^ | ^ |▶| ✅<sup>[3a]</sup><br>✅<sup>[3b]</sup> | ✅<br>all subordinate entities |
|Ordinate<br>Dimension|218|0, ‡1| ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|Point<br>Dimension|220|  0  | ^ | ^ |▶| ^ | ^ |
|Radius<br>Dimension|222|  0~1| ^ | ^ |▶| ✅<sup>[3a]</sup><br>✅<sup>[3b]</sup> | ^ |
|General Symbol|228|0~3, 5001~9999| ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|Select Area   |230|0, ‡1| ^ | ^ |▶| ^ | ^ |
|Associativity<br>Definition|302|5001~9999|St.| - |-| - | - |
|Line Font<br>Definition|304|  1  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
| ^            |304|  2  | ^ | ^ |▶| - | ^ |
|MACRO<br>Definition|‡306| 0  | ^ | ^ |-| ^ | ^ |
|Subfigure<br>Definition|308|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ✅<br>all associated entities |
|Text Font<br>Definition|310|  0  | ^ | ^ |-| ✅<sup>[3d]</sup> | - |
|Text Display<br>Definition|312|  0~1| ^ | ^ |▶| ^ | ^ |
|Color<br>Definition|314|  0  | ^ | ^ |-| - | ^ |
|Units Data    |‡316| 0  | ^ | ^ |-| ^ | ^ |
|Network<br>Subfigure<br>Definition|320|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup><br>✅<sup>[3b]</sup> | ✅<br>all associated entities<br>Text Display Templates<br>and Connect Points |
|Attribute Table<br>Definition|322|  0~1| ^ | ^ |▶| - | - |
| ^            |322|  2  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|Associativity<br>Instance|402|1, 7, 9, 12~15| ^ | ^ |-| ✅<sup>[3a]</sup> | ^ |
| ^            | ^ |  5  | ^ | ^ |-| ^  | ✅<br>all leaders |
| ^            | ^ |3~4, ‡19| ^ | ^ |-| ^ | - |
| ^            | ^ |  16 | ^ | ^ |-| ^ | ^ |
| ^            | ^ |18, ‡20| ^ | ^ |-| ^ | ^ |
| ^            | ^ |  21 | ^ | ^ |-| ^ | ^ |
|Drawing       |404|  0~1| ^ | ^ |-| ✅<sup>[3a]</sup> | ✅<br>all annotation entities |
|Property      |406|1~2, 5~17, ‡18~25| ^ | ^ |-| - | - |
| ^            | ^ |  3  | ^ | ^ |-| ^ | ^ |
| ^            | ^ |  ‡26| ^ | ^ |-| ^ | ^ |
| ^            | ^ |‡27, ‡31| ^ | ^ |-| ^ | ^ |
| ^            | ^ |‡28~30| ^ | ^ |-| ^ | ^ |
| ^            | ^ |‡32~35| ^ | ^ |-| ^ | ^ |
| ^            | ^ |  ‡36| ^ | ^ |-| ^ | ^ |
|Singular<br>Subfigure<br>Instance|408|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|View          |410|  0  | ^ | ^ |▶| ✅<sup>[3b]</sup> | ^ |
| ^            | ^ |  ‡1 | ^ | ^ |0| - | ^ |
|Rectangular<br>Array<br>Subfigure<br>Instance|412|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|Circular Array<br>Subfigure<br>Instance|414|  0  | ^ | ^ |▶| ^ | ^ |
|External<br>Reference|416|0~2, ‡3~4| ^ | ^ |-| - | ^ |
|Nodal Load<br>Constraint|418|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup> | ^ |
|Network<br>Subfigure<br>Instance|420|  0  | ^ | ^ |▶| ✅<sup>[3a]</sup><br>✅<sup>[3b]</sup><br>✅<sup>[3c]</sup> | ^ |
|Attribute Table<br>Instance|422|  0~1| ^ | ^ |-| - | ^ |
|Solid Instance|430|  0~1|CSG|[1]|▶| ✅<sup>[3a]</sup> | ^ |
|Vertex        |‡502| 1  |BRp| - |-| - | ^ |
|Edge          |‡504| 1  | ^ | ^ |-| ✅<sup>[3a]</sup> | ^ |
|Loop          |‡508| 0~1| ^ | ^ |-| ^ | ^ |
|Face          |‡510| 1  | ^ | ^ |-| ^ | ^ |
|Shell         |‡514| 1~2| ^ | ^ |-| ^ | ^ |

**Notes**

> **Note [1]**: Used to integrate primitive entities (other CSG entities) or B-Rep objects and combine them into more complex CSG entities.

> **Note [2]**: Analytical surface entities for B-Rep Solid models.

> **Note [3]**: A "✅" in the "Pointer" column indicates that the entity references other entities in the parameter data section, excluding additional pointers. However, for [3b] through [3d], whether it is a pointer depends on the parameter value.
> - **[3a]** Always a pointer regardless of the numeric value.
> - **[3b]** Not a pointer when the value is 0.
> - **[3c]** Not a pointer when the value is null.
> - **[3d]** When the value is negative, its absolute value is a pointer.

> **Note [4]**: The "Implicit" column indicates whether the pointer specification in the parameter data section satisfies the [implicit parent-child relationship](#transformation-matrix-application) with its target (Section 3.2.3).

> **Note [5]**: Also, any entity with Entity Use Flag = 00 or 01: all General Notes in text pointer field are implicitly dependent on the parent entity.

> **Note [6]**: Also used as analytical surface/curve entities for B-Rep solid models.

> **Note [7]**: The IGES 5.3 specification does not specify a class for the Direction type, but the other [2] class was C&S, so it is specified here as C&S.

In each Parameter Data section record, the parameters defined by each entity may be followed by two types of parameter groups. The following is quoted from Section 2.2.4.5.2.

>  The first group of parameters may contain pointers to any combination of one or more of the following entities: Associativity Instance Entity (Type 402), General Note Entity (Type 212), Text Template Entity (Type 312).
>
> - Pointers to associativity instances are called “back pointers” because they point back to the Associativity Instance Entity (Type 402) which references them; back pointers are used only when they are required by the associativity’s definition.
> - If an entity references associated text, a pointer to a General Note Entity (Type 212) may be included in the first group of pointers. The referenced note specifies the string and its display parameters.
> - If an entity itself contains a string to be displayed, a pointer to a Text Template Entity (Type 312) may be included in the first group of pointers. In this way, Text Template Entities provide display parameters for the first information item in the entity referencing them (see Section 4.75).
>
> The second group of parameters may contain pointers to one or more properties or attribute tables.