<?xml version="1.0" encoding="UTF-8"?>

<!DOCTYPE xsl:stylesheet [
<!ENTITY bull "&#160;">
<!ENTITY nbsp "&#160;">
]>

<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:exslt="http://exslt.org/common"
		xmlns:xhtml="http://www.w3.org/1999/xhtml"
		extension-element-prefixes="exslt">

<xsl:output method="xml"
	    version="1.0"
	    encoding="UTF-8"
	    doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
	    doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"/>

<xsl:template match="xhtml:html">
	<xsl:copy>
		<xsl:apply-templates select="@*|node()"/>
	</xsl:copy>
</xsl:template>

<xsl:template match="xhtml:eng">
	<xsl:if test="$lang = 'eng'">
		<xsl:apply-templates select="@*|node()"/>
	</xsl:if>
</xsl:template>

<xsl:template match="xhtml:rus">
	<xsl:if test="$lang = 'rus'">
		<xsl:apply-templates select="@*|node()"/>
	</xsl:if>
</xsl:template>

<xsl:template match="@*|node()">
	<xsl:copy>
		<xsl:apply-templates select="@*|node()"/>
	</xsl:copy>
</xsl:template>

</xsl:stylesheet>

