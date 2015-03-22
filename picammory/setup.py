from distutils.core import setup, Extension

cservice_mod = Extension("cservice",
            define_macros = [
                ('MAJOR_VERSION', '1'),
                ('MINOR_VERSION', '0')
            ],
            extra_compile_args=['-std=c99'],
            include_dirs = ['cservice'],
            sources = [
                'cservice/cservice.c',
                'cservice/motionservice.c'
            ]
        )

setup(
    name="picammory",
    version = '2.0',
    description = 'PiCammory - Raspberry Pi Camera',
    author = 'Pascal Mermoz',
    author_email = 'pascal@mermoz.net',
    url = 'http://www.mermoz.net',
    long_description = '''
Native code for accelerated.
''',
    ext_modules=[cservice_mod]
)
