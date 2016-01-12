const Zanata = imports.gi.Zanata;
const GLib = imports.gi.GLib;

let key_file = new GLib.KeyFile();
key_file.load_from_file(GLib.build_filenamev([GLib.get_user_config_dir(),
                                              'zanata.ini']),
                        GLib.KeyFileFlags.NONE);

let authorizer = new Zanata.KeyFileAuthorizer({ key_file: key_file });

let session = new Zanata.Session({ authorizer: authorizer,
                                   domain: 'translate_zanata_org' });
session.get_suggestions(['a'], 'en', 'ja', null,
                        function (s, res, d) {
                            let result = s.get_suggestions_finish(res);
                            print(result.length);
                            for (let index in result) {
				let suggestion = result[index];
                                print([suggestion.source_contents,
                                       suggestion.target_contents]);
                            }
                        });

let loop = GLib.MainLoop.new(null, false);
loop.run();
