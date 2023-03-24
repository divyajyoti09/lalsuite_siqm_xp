# LALPulsar

LAL routines for pulsar and continuous wave gravitational wave data
analysis.

## Acknowledgment

We request that any academic report, publication, or other academic
disclosure of results derived from the use of this software acknowledge
the use of the software by an appropriate acknowledgment or citation.

The whole LALSuite software suite can be cited with the DOI
[10.7935/GT1W-FZ16][doi]. An example BibTeX entry could look like this:

     @misc{lalsuite,
           author         = "{LIGO Scientific Collaboration}",
           title          = "{LIGO} {A}lgorithm {L}ibrary - {LALS}uite",
           howpublished   = "free software (GPL)",
           doi            = "10.7935/GT1W-FZ16",
           year           = "2018"
     }

The [Python/Octave interfaces to LALPulsar][swiglal] are described in this paper:

     @article{swiglal,
              title     = "{SWIGLAL: Python and Octave interfaces to the LALSuite gravitational-wave data analysis libraries}",
              author    = "Karl Wette",
              journal   = "SoftwareX",
              volume    = "12",
              pages     = "100634",
              year      = "2020",
              doi       = "10.1016/j.softx.2020.100634"
     }

In addition, some codes contained in this package may be directly based
on one or several scientific papers, which should be cited when using
those specific codes; some of these can be discovered through the
documentation.

## Basic Build Instructions

     ./configure --prefix=...
     make
     make install

Please read the [LALSuite install how-to][install] for more detailed
build instructions.

## Bug Reporting

Please visit the [LALSuite bug reporting system][bugs] (LIGO.org
authentication required to submit new issues), or use the [e-mail
helpdesk][helpdesk].

## For More Information

Please visit the [LALSuite project page][project].

[doi]:          https://doi.org/10.7935/GT1W-FZ16
[swiglal]:      https://lscsoft.docs.ligo.org/lalsuite/lalsuite/swiglal_tutorial.html
[helpdesk]:     mailto:contact+lscsoft-lalsuite-1438-issue-@support.ligo.org
[install]:      https://wiki.ligo.org/Computing/LALSuiteInstall
[bugs]:         https://git.ligo.org/lscsoft/lalsuite/issues/
[project]:      https://wiki.ligo.org/Computing/LALSuite
