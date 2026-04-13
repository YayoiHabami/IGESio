# Implementation Progress

## Entity Classes

- Abbreviations used in the "Class" column ("Class of the Entity"):
  - C&S: Curve and Surface Geometry class
  - CSG: Constructive Solid Geometry class
  - BRp: Boundary Representation (B-Rep) Solid class
  - An.: Annotation class
  - St.: Structure class

**Table 1**. Progress of Entity Classes Implementation in IGESio

|Name                      |Type|Forms|Class|Support|Graphics|
|:----------------------------|:-:|:---:|:-:|:-:|:-:|
|Null                         | 0 |  #  |St.|✅|🚫
|Circular Arc                 |100|  0  |C&S|✅|✅
|Composite<br>Curve           |102|  0  | ^ |✅|✅
|Conic Arc                    |104| 1~3 | ^ |✅|✅
|Copious Data                 |106| 1~3 | ^ |✅|✅
|Linear Path                  | ^ |11~12| ^ |✅|✅
| ^                           | ^ |  13 | ^ |✅|✅
|Centerline                   | ^ |20~21|An.|❌|❌
|Section                      | ^ |31~38| ^ |❌|❌
|Witness Line                 | ^ |  40 | ^ |❌|❌
|Closed Planar Loop           | ^ |  63 |C&S|✅|⚠️
|Plane                        |108|  0  | ^ |❌|❌
| ^                           |108|-1, 1| ^ |❌|❌
|Line                         |110|  0  |C&S|✅|✅
| ^                           | ^ | ‡1~2| ^ |✅|✅
|Parametric<br>Spline Curve   |112|  0  | ^ |✅|⚠️
|Parametric<br>Spline Surface |114|  0  | ^ |❌|❌
|Point                        |116|  0  |C&S|✅|⚠️
|Ruled Surface                |118|  0  | ^ |⚠️|⚠️
| ^                           | ^ |  1  | ^ |✅|⚠️
|Surface of<br>Revolution     |120|  0  | ^ |✅|⚠️
|Tabulated<br>Cylinder        |122|  0  | ^ |✅|⚠️
|Direction                    |‡123|0|C&S[7]|-|-
|Transformation<br>Matrix|124|0~1, 10~12|C&S|✅|🚫
|Flash                        |125| 0~4 | ^ |-|-
|Rational B<br>Spline Curve   |126| 0~5 | ^ |✅|✅
|Rational B<br>Spline Surface |128| 0~9 | ^ |✅|✅
|Offset Curve                 |130|  0  | ^ |❌|❌
|Connect Point                |132|  0  |St.|-|-
|Node                         |134|  0  | ^ |-|-
|Finite Element               |136|  0  | ^ |-|-
|Nodal<br>Displacement<br>and Rotation|138|  0  | ^ |-|-
|Offset Surface                       |140|  0  |C&S|❌|❌
|Boundary                             |141|  0  | ^ |❌|❌
|Curve on a<br>Parametric<br>Surface  |142|  0  | ^ |✅|✅
|Bounded<br>Surface                   |143|  0  | ^ |❌|❌
|Trimmed<br>Surface                   |144|  0  | ^ |✅|✅
|Nodal Results                        |‡146|0~34|St.|-|-
|Element Results                      |‡148|0~34| ^ |-|-
|Block                                |150|  0  |CSG|❌|❌
|Right Angular<br>Wedge               |152|  0  | ^ |❌|❌
|Right Circular<br>Cylinder           |154|  0  | ^ |❌|❌
|Right Circular<br>Cone               |156|  0  | ^ |❌|❌
|Sphere                               |158|  0  | ^ |❌|❌
|Torus                                |160|  0  | ^ |❌|❌
|Solid of<br>Revolution               |162|  0~1| ^ |❌|❌
|Solid of<br>Linear Extrusion         |164|  0  | ^ |❌|❌
|Ellipsoid                            |168|  0  | ^ |❌|❌
|Boolean Tree                         |180|  0~1|CSG|❌|❌
|Selected<br>Component                |‡182| 0  | ^ |-|-
|Solid Assembly                       |184|  0~1| ^ |❌|❌
|Manifold Solid<br>B-Rep Object       |‡186| 0  |BRp|❌|❌
|Plane Surface                        |‡190| 0~1|C&S|❌|❌
|Right Circular<br>Cylinder<br>Surface|‡192| 0~1| ^ |❌|❌
|Right Circular<br>Conical<br>Surface |‡194| 0~1| ^ |❌|❌
|Spherical<br>Surface              |‡196| 0~1| ^ |❌|❌
|Toroidal<br>Surface               |‡198| 0~1| ^ |❌|❌
|Angular<br>Dimension              |202|  0  |An.|-|-
|Curve<br>Dimension                |‡204| 0  | ^ |-|-
|Diameter<br>Dimension             |206|  0  | ^ |-|-
|Flag Note                         |208|  0  | ^ |-|-
|General Label                     |210|  0  | ^ |-|-
|General Note|212|0~8, <br> 100~102, <br> 105| ^ |-|-
|New<br>General Note               |‡213| 0  | ^ |-|-
|Leader Arrow                      |214| 1~12| ^ |-|-
|Linear<br>Dimension             |216|0, ‡1~2| ^ |-|-
|Ordinate<br>Dimension             |218|0, ‡1| ^ |-|-
|Point<br>Dimension                |220|  0  | ^ |-|-
|Radius<br>Dimension               |222| 0~1 | ^ |-|-
|General Symbol      |228|0~3, <br> 5001~9999| ^ |-|-
|Sectioned Area                    |230|0, ‡1| ^ |-|-
|Associativity<br>Definition   |302|5001~9999|St.|❌|-
|Line Font<br>Definition           |304|  1  | ^ |❌|-
| ^                                |304|  2  | ^ |❌|-
|MACRO<br>Definition               |‡306| 0  | ^ |-|-
|Subfigure<br>Definition           |308|  0  | ^ |❌|-
|Text Font<br>Definition           |310|  0  | ^ |❌|-
|Text Display<br>Definition        |312|  0~1| ^ |❌|-
|Color<br>Definition               |314|  0  | ^ |✅|🚫
|Units Data                        |‡316| 0  | ^ |-|-
|Network<br>Subfigure<br>Definition|320|  0  | ^ |-|-
|Attribute Table<br>Definition     |322|  0~1| ^ |-|-
| ^                                |322|  2  | ^ |-|-
|Associativity<br>Instance|402|1, 7, 9, 12~15| ^ |-|-
| ^                             | ^ |  5     | ^ |-|-
| ^                             | ^ |3~4, ‡19| ^ |-|-
| ^                             | ^ | 16     | ^ |-|-
| ^                             | ^ | 18, ‡20| ^ |-|-
| ^                             | ^ | 21     | ^ |-|-
|Drawing                        |404|  0~1   | ^ |-|-
|Property    |406|1~2, <br> 5~17, <br> ‡18~25| ^ |-|-
| ^                             | ^ |  3     | ^ |-|-
| ^                             | ^ | ‡26    | ^ |-|-
| ^                             | ^ |‡27, ‡31| ^ |-|-
| ^                             | ^ | ‡28~30 | ^ |-|-
| ^                             | ^ | ‡32~35 | ^ |-|-
| ^                             | ^ | ‡36    | ^ |-|-
|Singular<br>Subfigure<br>Instance |408|  0  | ^ |-|-
|View                              |410|  0  | ^ |-|-
| ^                                | ^ | ‡1  | ^ |-|-
|Rectangular<br>Array<br>Subfigure<br>Instance|412|  0  | ^ |-|-
|Circular Array<br>Subfigure<br>Instance      |414|  0  | ^ |-|-
|External<br>Reference                    |416|0~2, ‡3~4| ^ |-|-
|Nodal Load<br>Constraint                     |418|  0  | ^ |-|-
|Network<br>Subfigure<br>Instance             |420|  0  | ^ |-|-
|Attribute Table<br>Instance                  |422|  0~1| ^ |-|-
|Solid Instance|430|  0~1|CSG|❌|❌
|Vertex        |‡502| 1  |BRp|❌|❌
|Edge          |‡504| 1  | ^ |❌|❌
|Loop          |‡508| 0~1| ^ |❌|❌
|Face          |‡510| 1  | ^ |❌|❌
|Shell         |‡514| 1~2| ^ |❌|❌

> - ✅: Implemented
> - 🚫: Not applicable (no possible implementation)
> - ⚠️: Experimental / partial
>   - Graphics: uses general (curve/surface) shaders (not entity-specific)
> - ❌: Planned but not yet implemented
> - `-`: Not planned
