<%doc>
  Main template
</%doc>

## Page title
<%def name="pagetitle()">
PAWS
</%def>

## Title
<%def name="title()">
PAWS
</%def>

## Custom CSS
<%def name="js_slot()">
</%def>

## Custom Javascript
<%def name="css_slot()">
</%def>

<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>${self.pagetitle()}</title>
    <meta name="description" content="">
    <meta name="author" content="">

    ## HTML5 shim, for IE6-8 support of HTML elements
    <!--[if lt IE 9]>
      <script src="http://html5shim.googlecode.com/svn/trunk/html5.js"></script>
    <![endif]-->

    ## Stylesheets
    ${h.stylesheet_link(request.static_url("pawsapp:static/css/jq/bootstrap.css"), media="all")}
    ${h.stylesheet_link(request.static_url("pawsapp:static/css/normal.css"), media="all")}
    ${h.stylesheet_link(request.static_url("pawsapp:static/css/print.css"),  media="print")}
    <style type="text/css">
      body {
        padding-top: 60px;
      }
    </style>
    ${self.css_slot()}

    ## Fav and touch icons
    <link rel="shortcut icon" href="images/favicon.ico">
    ##<link rel="apple-touch-icon" href="images/apple-touch-icon.png">
    ##<link rel="apple-touch-icon" sizes="72x72" href="images/apple-touch-icon-72x72.png">
    ##<link rel="apple-touch-icon" sizes="114x114" href="images/apple-touch-icon-114x114.png">

    ## Javascript
    ${h.javascript_link(request.static_url("pawsapp:static/jq/jquery-1.7.1.min.js"))}
    ${h.javascript_link(request.static_url("pawsapp:static/js/base.js"))}
    ${h.javascript_link(request.static_url("pawsapp:static/js/init.js"))}
    ${self.js_slot()}

  </head>

  <body>

    <div class="topbar">
      <div class="topbar-inner">
        <div class="container-fluid">
          <a class="brand" href="#">PAWS</a>
          <ul class="nav">
            <li class="active"><a href="#">Home</a></li>
            <li><a href="#about">About</a></li>
            <li><a href="#contact">Contact</a></li>
          </ul>
          <p class="pull-right">Logged in as <a href="#">username</a></p>
        </div>
      </div>
    </div>

    <div class="container-fluid">

      <div class="sidebar">
        <div class="well">
	  ${self.left_column()}
        </div>
      </div>

      <div class="content">
	${self.main_column()}
        <footer>
          <p>© 2011-2012 MINES ParisTech</p>
        </footer>
      </div>

      ${self.dialogs()}

    </div>

  </body>
</html>
