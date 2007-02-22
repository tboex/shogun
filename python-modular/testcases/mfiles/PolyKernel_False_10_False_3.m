traindat = [0.945035748, 0.0370774978, 0.931247711, 0.494411174, 0.523830217, 0.235619063, 0.339382339, 0.6569858, 0.837912677, 0.294956086, 0.474830968, 0.768327994, 0.370436904;0.391517371, 0.939875266, 0.622856573, 0.834639144, 0.0679046406, 0.169853321, 0.729310611, 0.30303712, 0.703579232, 0.596973147, 0.848068885, 0.318785437, 0.869936953;0.0375441964, 0.0247972598, 0.278846998, 0.769930271, 0.0318626247, 0.0985144815, 0.696727452, 0.277652027, 0.727076957, 0.577443057, 0.524973939, 0.681337027, 0.0908529836;0.364539022, 0.183930228, 0.000450937738, 0.279865234, 0.916012331, 0.773058873, 0.839006434, 0.240856608, 0.24397913, 0.268046487, 0.56857789, 0.692318842, 0.736987362;0.742896554, 0.378438034, 0.844932853, 0.807723822, 0.243912961, 0.543922705, 0.110259276, 0.310097392, 0.378611764, 0.500338926, 0.274756271, 0.932046067, 0.733716067;0.107763923, 0.801544202, 0.114675126, 0.670983328, 0.28778946, 0.453403427, 0.764800627, 0.229091316, 0.512964867, 0.2520058, 0.632760398, 0.829575503, 0.0212436586;0.0769376861, 0.171634122, 0.937169352, 0.254131095, 0.427987459, 0.126494726, 0.473388972, 0.572769664, 0.224432856, 0.516367154, 0.0340966394, 0.408018201, 0.52198383;0.692779785, 0.255325251, 0.455021235, 0.738546094, 0.239131156, 0.567774524, 0.155424581, 0.770629348, 0.252924293, 0.390146663, 0.000588942862, 0.419429478, 0.42392414;0.748264859, 0.932795207, 0.49328391, 0.620799118, 0.721739082, 0.20671585, 0.227451915, 0.364226238, 0.893785022, 0.0378667691, 0.763998949, 0.727282014, 0.694657561;0.903612655, 0.589311948, 0.0892702188, 0.744829541, 0.695416925, 0.037888874, 0.0638856225, 0.240805025, 0.168822353, 0.675002773, 0.00338189637, 0.897448967, 0.608777916;0.802928018, 0.603200218, 0.290685712, 0.728557445, 0.882120677, 0.481568947, 0.850060302, 0.712435493, 0.675673114, 0.209101798, 0.919557389, 0.261686229, 0.486742433]
testdat = [0.403484749, 0.551873136, 0.584303155, 0.551979846, 0.173441871, 0.597881938, 0.30140053, 0.678611377, 0.711695168, 0.136749416, 0.859193242, 0.591451848, 0.88214285, 0.234923893, 0.297984976, 0.260538281, 0.620946601;0.866674215, 0.95929395, 0.049996229, 0.895137932, 0.324513944, 0.445331626, 0.0934668853, 0.085374837, 0.970423881, 0.639996907, 0.900628006, 0.0248239596, 0.834452337, 0.0771125052, 0.214087522, 0.340756172, 0.327524941;0.8840546, 0.471358459, 0.21775808, 0.47130286, 0.163084509, 0.756500283, 0.332221706, 0.176894659, 0.905149878, 0.317580623, 0.0219107245, 0.823260039, 0.282706999, 0.840332651, 0.538357831, 0.590730251, 0.539129643;0.419108837, 0.104266919, 0.908642169, 0.322446031, 0.283008317, 0.290116887, 0.805078857, 0.451900321, 0.455321715, 0.216752635, 0.590093888, 0.161940614, 0.457104839, 0.821698583, 0.565675792, 0.21266397, 0.154598981;0.671169435, 0.769039638, 0.124973014, 0.899435213, 0.962316751, 0.299551854, 0.150670041, 0.989394235, 0.181333965, 0.985215106, 0.565804758, 0.551896673, 0.517569021, 0.370503916, 0.0555178875, 0.560021434, 0.954117143;0.5541844, 0.436537901, 0.599106786, 0.127423758, 0.729196761, 0.198467446, 0.271509105, 0.629659244, 0.613063379, 0.0466802116, 0.863948271, 0.572945149, 0.118870172, 0.0217280257, 0.826880283, 0.739677052, 0.140239341;0.0314544619, 0.489466166, 0.501070376, 0.388904932, 0.80322006, 0.24175097, 0.357131338, 0.363009773, 0.932929779, 0.704451421, 0.00188063904, 0.432668121, 0.773989536, 0.788185654, 0.11352464, 0.499940196, 0.748201325;0.167576389, 0.333853869, 0.181479538, 0.844863657, 0.44555725, 0.270117815, 0.119120409, 0.854702838, 0.716223439, 0.849261895, 0.0980222774, 0.402106529, 0.6036537, 0.415821698, 0.323551921, 0.21223802, 0.465516475;0.409586165, 0.907499615, 0.932864072, 0.376326018, 0.970491668, 0.665693197, 0.999791265, 0.202752081, 0.623552012, 0.195296385, 0.100188971, 0.487683492, 0.222896406, 0.378783201, 0.122683512, 0.659037721, 0.00209594642;0.791664356, 0.629526823, 0.504206214, 0.448276839, 0.0205381769, 0.393852329, 0.73147744, 0.669554138, 0.778143559, 0.932939219, 0.867207539, 0.6591808, 0.716413601, 0.818913686, 0.867634606, 0.25088551, 0.999181702;0.746754342, 0.75725653, 0.108984959, 0.973743032, 0.80313005, 0.594319818, 0.907602677, 0.608616673, 0.27317797, 0.547040579, 0.395270769, 0.48474707, 0.225202949, 0.687257918, 0.809843456, 0.467791116, 0.560272107]
km_train = [76.8283806, 20.6479238, 23.0303212, 57.1130626, 31.0856342, 8.07549745, 10.0379952, 20.4116369, 29.1171935, 9.89548321, 17.4979957, 54.7811789, 36.0894687;20.6479238, 38.64084, 8.0446812, 39.5305718, 12.5706682, 3.75702076, 13.960832, 6.51624196, 20.877885, 5.58724144, 22.3964848, 27.880297, 21.0710772;23.0303212, 8.0446812, 42.1848037, 30.1742479, 7.56334755, 3.2869398, 8.3062002, 14.9245872, 21.4362745, 8.74773285, 9.24066191, 29.0812152, 20.9204882;57.1130626, 39.5305718, 30.1742479, 110.302638, 23.9063792, 11.43473, 32.5585715, 24.4481223, 50.5010179, 22.7831329, 36.207851, 85.0562861, 42.0487679;31.0856342, 12.5706682, 7.56334755, 23.9063792, 35.4305666, 6.85509394, 14.9460641, 10.9999664, 15.4079912, 4.48418146, 15.1176396, 32.7943228, 21.8354824;8.07549745, 3.75702076, 3.2869398, 11.43473, 6.85509394, 5.90581853, 7.24220108, 4.32960851, 4.94967503, 1.91742032, 5.72637265, 12.4987201, 7.19162317;10.0379952, 13.960832, 8.3062002, 32.5585715, 14.9460641, 7.24220108, 41.4283651, 9.85077671, 23.8091394, 7.35982202, 30.0393348, 26.7198809, 14.9341967;20.4116369, 6.51624196, 14.9245872, 24.4481223, 10.9999664, 4.32960851, 9.85077671, 14.3020927, 14.3541157, 5.21540824, 8.43997228, 18.8369623, 12.3931972;29.1171935, 20.877885, 21.4362745, 50.5010179, 15.4079912, 4.94967503, 23.8091394, 14.3541157, 46.2681067, 7.70098547, 34.5224583, 42.3924287, 20.4341988;9.89548321, 5.58724144, 8.74773285, 22.7831329, 4.48418146, 1.91742032, 7.35982202, 5.21540824, 7.70098547, 9.0268078, 4.41205169, 20.4292279, 11.0151255;17.4979957, 22.3964848, 9.24066191, 36.207851, 15.1176396, 5.72637265, 30.0393348, 8.43997228, 34.5224583, 4.41205169, 41.0598764, 26.4897004, 17.4513389;54.7811789, 27.880297, 29.0812152, 85.0562861, 32.7943228, 12.4987201, 26.7198809, 18.8369623, 42.3924287, 20.4292279, 26.4897004, 120.379361, 39.4870625;36.0894687, 21.0710772, 20.9204882, 42.0487679, 21.8354824, 7.19162317, 14.9341967, 12.3931972, 20.4341988, 11.0151255, 17.4513389, 39.4870625, 43.8549864]
km_test = [32.9178073, 50.4910582, 15.1424993, 53.7103186, 25.7718397, 18.7023152, 27.4451326, 43.4079268, 39.661819, 35.2429696, 30.8990102, 21.7640663, 33.0030016, 21.5970001, 15.2739693, 11.4823045, 34.3260678;26.3029762, 40.6157037, 9.71908631, 23.4017863, 26.1884292, 9.35789588, 16.2486841, 12.2207812, 31.3353667, 14.9646621, 21.0775987, 8.95046931, 11.4232078, 6.8884418, 11.0864256, 13.3673115, 8.26885763;13.6248841, 35.2025377, 6.8559841, 33.8650886, 26.3431183, 10.7479079, 5.84698723, 19.8728079, 39.3037712, 22.8502443, 11.1299221, 13.0887899, 30.0982738, 10.7155033, 2.35235123, 11.0338818, 24.9230046;71.7064223, 81.4426887, 16.5998028, 78.6708216, 45.2713566, 29.98174, 27.4746122, 49.9401041, 89.6590797, 52.9443162, 41.5723246, 38.4897181, 41.2276371, 33.5273135, 29.1816209, 29.6994445, 46.312634;16.8539045, 22.6181275, 20.336149, 20.2827916, 19.5825452, 10.7958392, 33.9928104, 19.9459219, 24.5912181, 13.0504429, 15.3900245, 12.244977, 14.5316907, 24.1441987, 15.4328728, 8.61767086, 13.8501635;6.26103227, 6.47462152, 4.41507262, 10.9024272, 9.63118983, 2.69095594, 5.18956872, 11.4877931, 7.58586437, 5.90301221, 5.959617, 3.73898496, 4.81425364, 5.58214158, 4.47836486, 3.62812858, 4.28442739;28.4813988, 24.273312, 10.9893318, 24.2149574, 18.9199229, 12.5066204, 16.2882142, 12.1124978, 40.266078, 9.19680022, 17.8254784, 11.5393278, 13.558647, 18.1657298, 18.0106952, 13.6113093, 9.72518236;11.0627074, 19.8364765, 6.02888681, 26.0159696, 16.102671, 8.65777322, 9.32287982, 17.9021931, 27.3995009, 15.3241266, 7.55874294, 10.727874, 16.2211651, 12.5760697, 6.83689731, 7.0383391, 14.705145;35.4497011, 48.3856173, 14.0031251, 35.627231, 27.1667462, 22.8085875, 19.5349483, 16.940327, 50.4579688, 12.4903365, 19.663945, 20.2904851, 19.660893, 14.4771284, 11.9393496, 18.4889263, 14.6774035;16.0477756, 16.2136607, 3.46346236, 17.3495683, 6.72236957, 5.85383128, 4.47921325, 11.1779891, 28.2703035, 17.8859989, 10.2712555, 8.91686583, 15.7625694, 12.0360834, 6.42288614, 5.62139495, 17.6532786;31.7802315, 35.2402994, 10.5907023, 26.4089237, 22.8482878, 15.5572579, 19.2680008, 9.86100524, 29.6525749, 6.62141649, 18.9058496, 10.3017897, 10.0612649, 9.96748457, 12.3456469, 14.1255031, 6.23351306;57.1104485, 62.3332031, 35.8010805, 48.2957357, 42.6781311, 26.1063046, 33.6214863, 56.655165, 83.4587802, 39.2558298, 47.094192, 44.4889425, 41.6684189, 39.1697064, 28.5171424, 30.1961447, 46.9235135;27.3863218, 43.2957628, 13.8293091, 42.7180195, 26.3899887, 12.9383287, 20.8263087, 22.2019576, 42.9543086, 33.3082515, 22.0605321, 11.0042241, 31.4958494, 21.7694698, 8.30080638, 10.928134, 23.179195]
functionname = 'test_poly_kernel'
kernelname = 'PolyKernel'
use_norm= 'False' 
size_= 10 
inhom= 'False' 
degree= 3 
