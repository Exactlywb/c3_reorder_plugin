# c3_reorder_plugin
C3 reorder IPA plugin for gcc

## How to apply a plugin
**|!| The plugin should be used on gcc release version 9.4.0**

The first you have to check your gcc-compiler supports plugins
```bash
> gcc -print-file-name=plugin
```

Then you should install required dependencies
```bash
> sudo apt-get install gcc-9-plugin-dev
```

After all the operations <path_to_plugin_dir>/include has to contain all the necessary files.

Before using the plugin it should be built
```bash
> g++ -I`gcc -print-file-name=plugin`/include -fPIC -fno-rtti -rdynamic -shared c3-ipa.cpp -o c3-ipa.so
```

Now you have *c3-ipa.so* shared file and it's finally usable.

To attach the *c3 reorder*
```bash
> g++ -fplugin=c3-ipa.so test.cpp -o test -fdump-passes
```

**-fdump-passes** is added to make sure you actually have a *ipa-c3_reorder*
```bash
...
   ipa-pure-const                                      :  ON
   ipa-c3_reorder                                      :  OFF
   ipa-free-fnsummary2                                 :  ON
...
```
