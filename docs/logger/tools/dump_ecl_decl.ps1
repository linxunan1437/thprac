<#
    dump_ecl_decl.ps1 —— 批量把各作 data\*.ecl 反编译成 .decl

    用途：为「ECL 白名单 CSV 生成」（docs/logger/ecl_whitelist_gen_guide.md）准备 B 路素材：
    每个段落实际落在哪个 ECL sub。CSV 第一列「段落入口函数」的 sub 名即来自 .decl。

    产物落在仓库外的 D:\workshop\reasm\exes\ecl_dump\thXX\*.decl（不进 git）。

    用法：
        powershell -NoProfile -ExecutionPolicy Bypass -File docs\logger\tools\dump_ecl_decl.ps1

    覆盖 th10–th18 共 9 作，每作 glob data\*.ecl 全量（含 default.ecl 与各 *bs/*mbs，
    一关 ≠ 一个 .ecl）。th10–17 带 eclmap，th18 无 map（dump 成 ins_*，不影响 sub 名）。

    游戏目录不硬编码：按 "[th<版本>]" 前缀在 $Src 下找，取其中【唯一含 data 子目录】的那个
    （例：D:\C P\C2H2 下有三个 "[th18]..."，只有 "[th18]" 带 data）。
#>

[CmdletBinding()]
param(
    [string]$Src    = 'D:\C P\C2H2',
    [string]$Thecl  = 'D:\C P\C2H2\拆包\THTK\thtk\thtk12\thecl.exe',
    [string]$MapDir = 'D:\C P\C2H2\拆包\THTK\MAP',
    [string]$Out    = 'D:\workshop\reasm\exes\ecl_dump',
    [int[]] $Versions = 10..18
)

if (-not (Test-Path -LiteralPath $Thecl)) { throw "找不到 thecl.exe: $Thecl" }
if (-not (Test-Path -LiteralPath $Src))   { throw "找不到游戏根目录: $Src" }
if (-not (Test-Path -LiteralPath $Out))   { New-Item -ItemType Directory -Path $Out -Force | Out-Null }

$failures = New-Object System.Collections.Generic.List[string]
$report   = New-Object System.Collections.Generic.List[string]
$totalOk  = 0
$totalAll = 0

foreach ($ver in $Versions) {
    $tag = "th$ver"

    # 定位拆包目录："[th<ver>]" 前缀，且含 data 子目录，且唯一。
    $cands = @(Get-ChildItem -LiteralPath $Src -Directory |
               Where-Object { $_.Name -match "^\[th$ver\]" })
    $picks = @($cands | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'data') })

    if ($picks.Count -ne 1) {
        $failures.Add("$tag : 期望恰好 1 个含 data 的 '[th$ver]*' 目录，实际 $($picks.Count)（候选：$($cands.Name -join ' | ')）")
        $report.Add(("{0,-6} 期望  ?  实际  0  失败  ?   (游戏目录定位失败)" -f $tag))
        continue
    }

    $dataDir = Join-Path $picks[0].FullName 'data'
    $outDir  = Join-Path $Out $tag
    if (-not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

    # th18 无 ECL_MAP_18.txt -> $mapArgs 为空，自动跳过 -m
    $map     = Join-Path $MapDir ("ECL_MAP_$ver.txt")
    $mapArgs = @()
    if (Test-Path -LiteralPath $map) { $mapArgs = @('-m', $map) }

    $ecls = @(Get-ChildItem -LiteralPath $dataDir -Filter *.ecl -File | Sort-Object Name)
    $ok   = 0
    foreach ($f in $ecls) {
        $decl = Join-Path $outDir ($f.BaseName + '.decl')
        if (Test-Path -LiteralPath $decl) { Remove-Item -LiteralPath $decl -Force }   # 防旧产物干扰判成败

        & $Thecl -d $ver @mapArgs $f.FullName $decl | Out-Null
        $code = $LASTEXITCODE

        # 判别式：产物存在、非空、且含至少一个 sub 声明（^void）。
        $good = $false
        if ((Test-Path -LiteralPath $decl) -and ((Get-Item -LiteralPath $decl).Length -gt 0)) {
            $txt = Get-Content -LiteralPath $decl -Raw
            if ($null -ne $txt -and $txt -match '(?m)^void\s') { $good = $true }
        }

        if ($code -ne 0 -or -not $good) {
            $failures.Add("$tag : dump 失败 ($($f.Name)) exit=$code")
        } else {
            $ok++
        }
    }

    $totalOk  += $ok
    $totalAll += $ecls.Count
    $report.Add(("{0,-6} 期望 {1,3}  实际 {2,3}  失败 {3}   <- {4}" -f $tag, $ecls.Count, $ok, ($ecls.Count - $ok), $picks[0].Name))
}

# 汇总
$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("ECL .decl dump 汇总  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$summary.Add("thecl : $Thecl")
$summary.Add("输出  : $Out")
$summary.Add("")
$summary.AddRange($report)
$summary.Add("")
$summary.Add(("合计：期望 {0}  成功 {1}  失败 {2}" -f $totalAll, $totalOk, $failures.Count))
if ($failures.Count -gt 0) {
    $summary.Add("")
    $summary.Add("失败清单：")
    $summary.AddRange($failures)
}

$summary | ForEach-Object { Write-Host $_ }
if ($failures.Count -eq 0) {
    $reportPath = Join-Path $Out '_dump_report.txt'
    $summary | Out-File -LiteralPath $reportPath -Encoding utf8
    Write-Host ""
    Write-Host "报告：$reportPath"
} else {
    Write-Host ""
    Write-Host "（有失败，未写 _dump_report.txt）"
}

if ($failures.Count -gt 0) { exit 1 } else { exit 0 }
