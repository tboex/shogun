traindat = [0.945035748, 0.0370774978, 0.931247711, 0.494411174, 0.523830217, 0.235619063, 0.339382339, 0.6569858, 0.837912677, 0.294956086, 0.474830968, 0.768327994, 0.370436904;0.391517371, 0.939875266, 0.622856573, 0.834639144, 0.0679046406, 0.169853321, 0.729310611, 0.30303712, 0.703579232, 0.596973147, 0.848068885, 0.318785437, 0.869936953;0.0375441964, 0.0247972598, 0.278846998, 0.769930271, 0.0318626247, 0.0985144815, 0.696727452, 0.277652027, 0.727076957, 0.577443057, 0.524973939, 0.681337027, 0.0908529836;0.364539022, 0.183930228, 0.000450937738, 0.279865234, 0.916012331, 0.773058873, 0.839006434, 0.240856608, 0.24397913, 0.268046487, 0.56857789, 0.692318842, 0.736987362;0.742896554, 0.378438034, 0.844932853, 0.807723822, 0.243912961, 0.543922705, 0.110259276, 0.310097392, 0.378611764, 0.500338926, 0.274756271, 0.932046067, 0.733716067;0.107763923, 0.801544202, 0.114675126, 0.670983328, 0.28778946, 0.453403427, 0.764800627, 0.229091316, 0.512964867, 0.2520058, 0.632760398, 0.829575503, 0.0212436586;0.0769376861, 0.171634122, 0.937169352, 0.254131095, 0.427987459, 0.126494726, 0.473388972, 0.572769664, 0.224432856, 0.516367154, 0.0340966394, 0.408018201, 0.52198383;0.692779785, 0.255325251, 0.455021235, 0.738546094, 0.239131156, 0.567774524, 0.155424581, 0.770629348, 0.252924293, 0.390146663, 0.000588942862, 0.419429478, 0.42392414;0.748264859, 0.932795207, 0.49328391, 0.620799118, 0.721739082, 0.20671585, 0.227451915, 0.364226238, 0.893785022, 0.0378667691, 0.763998949, 0.727282014, 0.694657561;0.903612655, 0.589311948, 0.0892702188, 0.744829541, 0.695416925, 0.037888874, 0.0638856225, 0.240805025, 0.168822353, 0.675002773, 0.00338189637, 0.897448967, 0.608777916;0.802928018, 0.603200218, 0.290685712, 0.728557445, 0.882120677, 0.481568947, 0.850060302, 0.712435493, 0.675673114, 0.209101798, 0.919557389, 0.261686229, 0.486742433]
testdat = [0.403484749, 0.551873136, 0.584303155, 0.551979846, 0.173441871, 0.597881938, 0.30140053, 0.678611377, 0.711695168, 0.136749416, 0.859193242, 0.591451848, 0.88214285, 0.234923893, 0.297984976, 0.260538281, 0.620946601;0.866674215, 0.95929395, 0.049996229, 0.895137932, 0.324513944, 0.445331626, 0.0934668853, 0.085374837, 0.970423881, 0.639996907, 0.900628006, 0.0248239596, 0.834452337, 0.0771125052, 0.214087522, 0.340756172, 0.327524941;0.8840546, 0.471358459, 0.21775808, 0.47130286, 0.163084509, 0.756500283, 0.332221706, 0.176894659, 0.905149878, 0.317580623, 0.0219107245, 0.823260039, 0.282706999, 0.840332651, 0.538357831, 0.590730251, 0.539129643;0.419108837, 0.104266919, 0.908642169, 0.322446031, 0.283008317, 0.290116887, 0.805078857, 0.451900321, 0.455321715, 0.216752635, 0.590093888, 0.161940614, 0.457104839, 0.821698583, 0.565675792, 0.21266397, 0.154598981;0.671169435, 0.769039638, 0.124973014, 0.899435213, 0.962316751, 0.299551854, 0.150670041, 0.989394235, 0.181333965, 0.985215106, 0.565804758, 0.551896673, 0.517569021, 0.370503916, 0.0555178875, 0.560021434, 0.954117143;0.5541844, 0.436537901, 0.599106786, 0.127423758, 0.729196761, 0.198467446, 0.271509105, 0.629659244, 0.613063379, 0.0466802116, 0.863948271, 0.572945149, 0.118870172, 0.0217280257, 0.826880283, 0.739677052, 0.140239341;0.0314544619, 0.489466166, 0.501070376, 0.388904932, 0.80322006, 0.24175097, 0.357131338, 0.363009773, 0.932929779, 0.704451421, 0.00188063904, 0.432668121, 0.773989536, 0.788185654, 0.11352464, 0.499940196, 0.748201325;0.167576389, 0.333853869, 0.181479538, 0.844863657, 0.44555725, 0.270117815, 0.119120409, 0.854702838, 0.716223439, 0.849261895, 0.0980222774, 0.402106529, 0.6036537, 0.415821698, 0.323551921, 0.21223802, 0.465516475;0.409586165, 0.907499615, 0.932864072, 0.376326018, 0.970491668, 0.665693197, 0.999791265, 0.202752081, 0.623552012, 0.195296385, 0.100188971, 0.487683492, 0.222896406, 0.378783201, 0.122683512, 0.659037721, 0.00209594642;0.791664356, 0.629526823, 0.504206214, 0.448276839, 0.0205381769, 0.393852329, 0.73147744, 0.669554138, 0.778143559, 0.932939219, 0.867207539, 0.6591808, 0.716413601, 0.818913686, 0.867634606, 0.25088551, 0.999181702;0.746754342, 0.75725653, 0.108984959, 0.973743032, 0.80313005, 0.594319818, 0.907602677, 0.608616673, 0.27317797, 0.547040579, 0.395270769, 0.48474707, 0.225202949, 0.687257918, 0.809843456, 0.467791116, 0.560272107]
km_train = [1.24262331, 0.801907728, 0.83163406, 1.12566912, 0.919077598, 0.586437902, 0.630542839, 0.798837077, 0.899253439, 0.627544612, 0.758860075, 1.11013568, 0.965959903;0.801907728, 0.988206689, 0.585690998, 0.99573392, 0.679649586, 0.454408871, 0.703832587, 0.545964834, 0.804873761, 0.518678305, 0.823933617, 0.886335345, 0.807348759;0.83163406, 0.585690998, 1.01753873, 0.910006346, 0.573768835, 0.434606469, 0.59197005, 0.719669413, 0.811986308, 0.602278611, 0.613385205, 0.898882902, 0.805420862;1.12566912, 0.99573392, 0.910006346, 1.4018264, 0.842048043, 0.658527992, 0.933370456, 0.848361151, 1.08043584, 0.828647996, 0.967014945, 1.28548716, 1.01644378;0.919077598, 0.679649586, 0.573768835, 0.842048043, 0.9600451, 0.555268972, 0.720014456, 0.650073889, 0.727356979, 0.482014381, 0.722759151, 0.935617835, 0.816995881;0.586437902, 0.454408871, 0.434606469, 0.658527992, 0.555268972, 0.528354774, 0.565530192, 0.476411035, 0.498147358, 0.363137733, 0.52294836, 0.678350449, 0.564210604;0.630542839, 0.703832587, 0.59197005, 0.933370456, 0.720014456, 0.565530192, 1.01141999, 0.62659813, 0.840904807, 0.568575367, 0.908648067, 0.873863853, 0.719823838;0.798837077, 0.545964834, 0.719669413, 0.848361151, 0.650073889, 0.476411035, 0.62659813, 0.709521346, 0.710380586, 0.506907098, 0.59513105, 0.777742609, 0.676436016;0.899253439, 0.804873761, 0.811986308, 1.08043584, 0.727356979, 0.498147358, 0.840904807, 0.710380586, 1.04936404, 0.577228418, 0.951771817, 1.01920537, 0.7991313;0.627544612, 0.518678305, 0.602278611, 0.828647996, 0.482014381, 0.363137733, 0.568575367, 0.506907098, 0.577228418, 0.608616424, 0.479415937, 0.799066494, 0.650372375;0.758860075, 0.823933617, 0.613385205, 0.967014945, 0.722759151, 0.52294836, 0.908648067, 0.59513105, 0.951771817, 0.479415937, 1.00841233, 0.871347291, 0.758184999;1.11013568, 0.886335345, 0.898882902, 1.28548716, 0.935617835, 0.678350449, 0.873863853, 0.777742609, 1.01920537, 0.799066494, 0.871347291, 1.44327674, 0.995368468;0.965959903, 0.807348759, 0.805420862, 1.01644378, 0.816995881, 0.564210604, 0.719823838, 0.676436016, 0.7991313, 0.650372375, 0.758184999, 0.995368468, 1.03079411]
km_test = [0.936790697, 1.0803648, 0.723155106, 1.1028544, 0.86340404, 0.775885068, 0.881699742, 1.02727949, 0.996834696, 0.958347692, 0.917234663, 0.816104206, 0.937598167, 0.814010637, 0.725241934, 0.659440004, 0.949963579;0.869295104, 1.00476291, 0.623793362, 0.836081485, 0.868031368, 0.615968267, 0.740352138, 0.673284463, 0.921532232, 0.72031298, 0.807432042, 0.606895903, 0.658306731, 0.556167919, 0.65177263, 0.693713734, 0.591081601;0.698141105, 0.957981069, 0.555293006, 0.945691913, 0.869737101, 0.645070105, 0.526594499, 0.791745034, 0.993825976, 0.829460837, 0.652623905, 0.688861798, 0.909241952, 0.644421162, 0.38874635, 0.650741312, 0.853818843;1.21437165, 1.26701854, 0.74564695, 1.25247817, 1.04177382, 0.908066974, 0.882015316, 1.07642081, 1.30826893, 1.0975864, 1.01259017, 0.986916734, 1.00978384, 0.942537246, 0.899916207, 0.905207989, 1.04970056;0.749432368, 0.82664267, 0.797851088, 0.797152684, 0.787871363, 0.646027599, 0.946879309, 0.792714814, 0.850013096, 0.688188408, 0.727074153, 0.673728514, 0.713297965, 0.844831053, 0.727748292, 0.599278785, 0.701967873;0.538742103, 0.544799957, 0.479525331, 0.648146731, 0.621907199, 0.406569191, 0.506068562, 0.659545059, 0.57433766, 0.528271073, 0.529954258, 0.453680566, 0.493562239, 0.518520445, 0.481805878, 0.449151815, 0.47474806;0.892659932, 0.846334313, 0.649864329, 0.845655554, 0.778882698, 0.678493345, 0.740952032, 0.671289993, 1.00187156, 0.612413168, 0.763564979, 0.660529837, 0.697007933, 0.768392681, 0.766200495, 0.697909169, 0.623923755;0.651307498, 0.79126225, 0.531999609, 0.866121744, 0.738127823, 0.600206929, 0.615199015, 0.764658785, 0.881210818, 0.726034927, 0.573652374, 0.644669056, 0.739933945, 0.67974692, 0.554777222, 0.560173206, 0.716124789;0.960217895, 1.06513429, 0.704542603, 0.961818131, 0.878708467, 0.828956484, 0.787232518, 0.75071115, 1.08012875, 0.678198746, 0.788961517, 0.797253461, 0.788920696, 0.712404103, 0.668075932, 0.772922885, 0.715674178;0.737288088, 0.739819822, 0.442251275, 0.756708294, 0.551661993, 0.526799883, 0.481836301, 0.653562056, 0.890449087, 0.764428146, 0.635389618, 0.606135444, 0.732894181, 0.669875353, 0.543345003, 0.519733012, 0.761098258;0.92587272, 0.958323488, 0.641909578, 0.870460704, 0.829437162, 0.72969822, 0.783630182, 0.626814931, 0.90473156, 0.548886528, 0.778689531, 0.636018619, 0.631029696, 0.629062978, 0.675569789, 0.706589064, 0.53795163;1.12565195, 1.15896971, 0.963380048, 1.06447434, 1.02148988, 0.867123061, 0.9434189, 1.12265274, 1.27738834, 0.993421733, 1.05557246, 1.03573733, 1.01336977, 0.992694712, 0.893033199, 0.910226417, 1.05429572;0.881069509, 1.02639391, 0.701615374, 1.02180802, 0.870252617, 0.686212033, 0.804210433, 0.821541221, 1.02368855, 0.940479964, 0.819793111, 0.650157752, 0.92310275, 0.81617174, 0.591841885, 0.648655751, 0.833422177]
functionname = 'test_linear_kernel'
kernelname = 'LinearKernel'
bool1= 'False' 
