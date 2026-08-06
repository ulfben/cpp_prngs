[CmdletBinding()]
param(
	[string] $InputFile,
	[string] $OutputFile
)

$ErrorActionPreference = 'Stop'

$scriptDirectory = Split-Path -Parent $PSCommandPath
$repositoryRoot = [System.IO.Path]::GetFullPath(
	(Join-Path $scriptDirectory '..')
)

if(-not $InputFile -and -not $OutputFile){
	$generatedDirectory = Join-Path $scriptDirectory 'generated'
	$benchmarkNames = @(
		'engine_comparison',		
		'random_bounded_integer',
		'random_bounded_float'
	)

	foreach($benchmarkName in $benchmarkNames){
		& $PSCommandPath `
			-InputFile (Join-Path $scriptDirectory "$benchmarkName.cpp") `
			-OutputFile (Join-Path $generatedDirectory "$benchmarkName.quickbench.cpp")
	}

	exit 0
}

if(-not $InputFile -or -not $OutputFile){
	throw 'Specify both -InputFile and -OutputFile, or specify neither to generate every benchmark.'
}

$InputFile = [System.IO.Path]::GetFullPath($InputFile)
$OutputFile = [System.IO.Path]::GetFullPath($OutputFile)
$repositoryPrefix = $repositoryRoot.TrimEnd(
	[System.IO.Path]::DirectorySeparatorChar,
	[System.IO.Path]::AltDirectorySeparatorChar
) + [System.IO.Path]::DirectorySeparatorChar

$expandedFiles = [System.Collections.Generic.HashSet[string]]::new(
	[System.StringComparer]::OrdinalIgnoreCase
)
$output = [System.Collections.Generic.List[string]]::new()

function Remove-CppComments {
	param(
		[Parameter(Mandatory)]
		[string] $Text
	)

	$result = [System.Text.StringBuilder]::new($Text.Length)
	$state = 'code'

	for($i = 0; $i -lt $Text.Length; ++$i){
		$character = $Text[$i]
		$previous = if($i -gt 0){ $Text[$i - 1] } else{ [char]0 }
		$next = if($i + 1 -lt $Text.Length){ $Text[$i + 1] } else{ [char]0 }

		switch($state){
			'code' {
				if($character -eq '/' -and $next -eq '/'){
					[void] $result.Append(' ')
					$state = 'line-comment'
					++$i
				} elseif($character -eq '/' -and $next -eq '*'){
					[void] $result.Append(' ')
					$state = 'block-comment'
					++$i
				} else{
					[void] $result.Append($character)
					if($character -eq '"'){
						$state = 'string'
					} elseif(
						$character -eq "'" -and
						-not (
							[char]::IsLetterOrDigit($previous) -and
							[char]::IsLetterOrDigit($next)
						)
					){
						$state = 'character'
					}
				}
			}
			'line-comment' {
				if($character -eq "`r" -or $character -eq "`n"){
					[void] $result.Append($character)
					$state = 'code'
				}
			}
			'block-comment' {
				if($character -eq '*' -and $next -eq '/'){
					$state = 'code'
					++$i
				} elseif($character -eq "`r" -or $character -eq "`n"){
					[void] $result.Append($character)
				}
			}
			'string' {
				[void] $result.Append($character)
				if($character -eq '\\' -and $i + 1 -lt $Text.Length){
					++$i
					[void] $result.Append($Text[$i])
				} elseif($character -eq '"'){
					$state = 'code'
				}
			}
			'character' {
				[void] $result.Append($character)
				if($character -eq '\\' -and $i + 1 -lt $Text.Length){
					++$i
					[void] $result.Append($Text[$i])
				} elseif($character -eq "'"){
					$state = 'code'
				}
			}
		}
	}

	if($state -eq 'block-comment'){
		throw 'Unterminated block comment in generated source.'
	}

	return $result.ToString()
}

function Add-SourceFile {
	param(
		[Parameter(Mandatory)]
		[string] $Path,

		[bool] $IsIncludedFile = $false
	)

	$resolvedPath = [System.IO.Path]::GetFullPath($Path)
	if(-not (Test-Path -LiteralPath $resolvedPath -PathType Leaf)){
		throw "Source file not found: $resolvedPath"
	}

	if($IsIncludedFile -and -not $expandedFiles.Add($resolvedPath)){
		return
	}

	if(-not $resolvedPath.StartsWith(
		$repositoryPrefix,
		[System.StringComparison]::OrdinalIgnoreCase
	)){
		throw "Source file is outside the repository: $resolvedPath"
	}

	$relativePath = $resolvedPath.Substring(
		$repositoryPrefix.Length
	).Replace('\', '/')

	if($IsIncludedFile){
		$output.Add('')
		$output.Add("// ===== BEGIN $relativePath =====")
	}

	$sourceDirectory = Split-Path -Parent $resolvedPath
	$validationDepth = 0
	foreach($line in [System.IO.File]::ReadLines($resolvedPath)){
		if($validationDepth -gt 0){
			if($line -match '^\s*#\s*(if|ifdef|ifndef)\b'){
				++$validationDepth
			} elseif($line -match '^\s*#\s*endif\b'){
				--$validationDepth
			}
			continue
		}

		if($line -match '^\s*#\s*(if|ifdef|ifndef)\s+(defined\s*\(\s*)?VALIDATE_PRNGS\b'){
			$validationDepth = 1
			continue
		}

		$includeMatch = [regex]::Match(
			$line,
			'^\s*#\s*include\s*"([^"]+)"'
		)

		if($includeMatch.Success){
			$includePath = [System.IO.Path]::GetFullPath(
				(Join-Path $sourceDirectory $includeMatch.Groups[1].Value)
			)

			if(-not $includePath.StartsWith(
				$repositoryPrefix,
				[System.StringComparison]::OrdinalIgnoreCase
			)){
				throw "Quoted include escapes the repository: $includePath"
			}

			Add-SourceFile -Path $includePath -IsIncludedFile $true
			continue
		}

		if($IsIncludedFile -and $line -match '^\s*#\s*pragma\s+once\s*$'){
			continue
		}

		$output.Add($line)
	}

	if($validationDepth -ne 0){
		throw "Unterminated VALIDATE_PRNGS block in $resolvedPath"
	}

	if($IsIncludedFile){
		$output.Add("// ===== END $relativePath =====")
	}
}

$output.Add('// This file is generated. Do not edit it directly.')
$output.Add('// Source: https://github.com/ulfben/cpp_prngs')
$output.Add('// Regenerate with benchmarks/generate-quickbench.ps1')
$output.Add('')

Add-SourceFile -Path $InputFile

$outputDirectory = Split-Path -Parent $OutputFile
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$generatedSource = [string]::Join([Environment]::NewLine, $output)
$generatedSource = Remove-CppComments -Text $generatedSource
$generatedSource = [regex]::Replace(
	$generatedSource,
	'(?m)^[\t ]*\r?\n',
	''
)
$generatedSource = [regex]::Replace(
	$generatedSource,
	'(?m)^[\t ]+',
	''
)
[System.IO.File]::WriteAllText(
	$OutputFile,
	$generatedSource + [Environment]::NewLine,
	[System.Text.UTF8Encoding]::new($false)
)

Write-Host "Generated $OutputFile"
